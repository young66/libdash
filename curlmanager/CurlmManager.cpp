#include "CurlmManager.h"

#include <iostream>
#include <utility>
#include <cstdio>
using namespace xmb;
using namespace std;



CurlmManager::CurlmManager() : transfer(0), stopSignal(0), running(0)
{
    cout << "Constructing CurlmManager" << endl;

    pthread_mutex_init(&pending_lock, NULL);
    pthread_cond_init(&pending_cond, NULL);

    curl_global_init(CURL_GLOBAL_ALL);
    curlm = curl_multi_init();
    curl_multi_setopt(curlm, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    curl_multi_setopt(curlm, CURLMOPT_PUSHFUNCTION, CurlmManager::pushCallback);
    curl_multi_setopt(curlm, CURLMOPT_PUSHDATA, (void *)this);
}



CurlmManager::~CurlmManager()
{
    cout << "Deconstructing CurlmManager" << endl;
    if(transfer > 0){
        /* try joining the fetching thread */
        pthread_join(fetchThread, NULL);
    }
    curl_multi_cleanup(curlm);
    curl_global_cleanup();

    pthread_cond_destroy(&pending_cond);
    pthread_mutex_destroy(&pending_lock);
}

void CurlmManager::stopFetching()
{
    stopSignal = 1;
    pthread_cond_signal(&pending_cond);
}

void CurlmManager::startFetching()
{
    if(running)
        return;

    /* create thread here */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int ret;
    ret = pthread_create(&fetchThread, &attr, fetchUrls, this);
    if(ret){
        cerr << "create thread error!" << endl;
        return;
    }

    stopSignal = 0;
}

int CurlmManager::pushCallback(CURL *parent, CURL *self, size_t headers_nr, struct curl_pushheaders *headers, void *userdata)
{

    /* a cache should be implemented to hold these content*/
    // (void)parent;

    // char* headp;
    // headp = curl_pushheader_byname(headers, ":path");
    // const char* filename = strrchr(headp, '/') + 1;

    // printf("[menxiao] push stream dump to file %s\n", filename);

    // FILE *fp = fopen(filename, "wb");

    // curl_easy_setopt(self, CURLOPT_WRITEDATA, fp);

    /* int i; */
    /* for(i = 0; i < headers_nr; i++){ */
    /*     headp = curl_pushheader_bynum(headers, i); */
    /*     printf("[headers] %s\n", headp); */
    /* } */

    // (*((int *)userdata))++;
    return CURL_PUSH_OK;
}


// void CurlmManager::commitEasyHandle(CURL *easy)
// {
//     pthread_mutex_lock(&pending_lock);
//     cout << "add a curl handle into the pending queue" << endl;
//     pending.push_back(easy);
//     pthread_cond_signal(&pending_cond);
//     pthread_mutex_unlock(&pending_lock);
// }

void CurlmManager::commitHttpTransaction(string url, size_t (*onprogress)(void *, size_t, size_t, void *), void (*oncomplete)(void *), void *usrp)
{
    // char pushdirective[256];
    // struct curl_slist *headers = NULL;
    // memset(pushdirective, '\0', 256);
    // snprintf(pushdirective, 256, "PushDirective: urn:mpeg:dash:fdh:2016:push-next;%d", k);
    // headers = curl_slist_append(headers, pushdirective);
    // curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    CURL *easy = curl_easy_init();

    curl_easy_setopt(easy, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(easy, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, usrp); // opened FILE *fp is available
    if(onprogress != NULL){
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, onprogress);
    }

    pthread_mutex_lock(&pending_lock);
    cout << "add an HTTP transaction into the pending queue" << endl;

    pending.push_back({url, onprogress, oncomplete, usrp, easy});
    //pending.push_back(easy);
    pthread_cond_signal(&pending_cond);
    pthread_mutex_unlock(&pending_lock);

}


int CurlmManager::checkEasyHandleDoneOrFailed(CURL *easy)
{
    /* should provide failed messages */
    map<CURL *, HttpTrans_t>::iterator iter;
    iter = done.find(easy);
    if(iter != done.end())
        return 0;

    iter = failed.find(easy);
    if(iter != failed.end())
        return 1;

    return -1;
}


void *CurlmManager::fetchUrls(void *data)
{
    CurlmManager *cm = (CurlmManager *)data;
    cm->running = 1;

RESELECT:
    /* fetch the handles from the pending */
    // add handle into handles
//    printf("[fetchThread] Try checking the pending queue (queue size %lu)\n", cm->pending.size());
    pthread_mutex_lock(&cm->pending_lock);
    // detect if there is any pending curl requests
    if(cm->pending.size() > 0){
        for(auto const& trans : cm->pending){
            cout << "Start fetching a url " << trans.url << endl;
            cm->transfer++;
            curl_multi_add_handle(cm->curlm, trans.curl);
            cm->ongoing.emplace(trans.curl, trans);
        }
        cout << "clean up the pending queue" << endl;
        cm->pending.clear();
    }
    pthread_mutex_unlock(&cm->pending_lock);

    int transferrings;
    curl_multi_perform(cm->curlm, &transferrings);

    struct timeval timeout;
    long curlto;
    fd_set fdread, fdwrite, fdexcep;
    int maxfd;

    maxfd = -1;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);


    curl_multi_timeout(cm->curlm, &curlto);
    if(curlto < 0)
        curlto = 1000;

    timeout.tv_sec = curlto / 1000;
    timeout.tv_usec = (curlto % 1000) * 1000;

    CURLMcode ret;
    ret = curl_multi_fdset(cm->curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
    if(ret != CURLM_OK){
        cerr << "multi fdset failed!" << endl;
        goto FETCHING_END;
    }

    int rc;
    if(maxfd == -1){
        cerr << "[fetchThread] failed to set fdset, wait for 100ms" << endl;
        struct timeval wait = { 0, 100 * 1000 };
        rc = select(0, NULL, NULL, NULL, &wait);
    }else{
        rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }

    switch(rc){
    case -1:
        break;
    case 0:
    default:
        curl_multi_perform(cm->curlm, &transferrings);
        break;
    }

    struct CURLMsg *m;
    do{
        int msgq = 0;
        m = curl_multi_info_read(cm->curlm, &msgq);
        // How about the other types of messages?
        if(m && (m->msg == CURLMSG_DONE)) {
            CURL *e = m->easy_handle;
            printf("the complete curl handle is %p\n", e);
            cm->transfer--;
            curl_multi_remove_handle(cm->curlm, e);
            curl_easy_cleanup(e);
            printf("the complete curl handle is %p after cleanup\n", e);
            cout << "one transfer done! (" << cm->transfer << " remain)" << endl;
            auto trans = cm->ongoing.find(e)->second;
            trans.oncomplete(trans.usrp);
            cm->done.emplace(e, trans);
        }
    }while(m);

    if(cm->transfer > 0 || cm->pending.size() > 0)
        goto RESELECT;

    /* sleep on the pending queue and have lock on it*/
    pthread_mutex_lock(&cm->pending_lock);
    while(cm->pending.size() == 0 && !cm->stopSignal){
        cout << "no pending transfer request! sleep" << endl;
        pthread_cond_wait(&cm->pending_cond, &cm->pending_lock);
        cout << "Wake up in fetching thread" << endl;
    }
    pthread_mutex_unlock(&cm->pending_lock);

    cout << "we got " << cm->pending.size() << " curl handles new!" << endl;
    if(!cm->stopSignal)
        goto RESELECT;

    cout << "receive stop signal, fetching thread terminates." << endl;
FETCHING_END:
    cm->running = 0;
    pthread_exit(0);
}

