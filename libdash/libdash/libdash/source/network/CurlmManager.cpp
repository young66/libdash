#include "CurlmManager.h"

#include <iostream>
#include <utility>
#include <cstdio>
using namespace dash::network;
using namespace std;



CurlmManager::CurlmManager() : transfer(0), stopSignal(0), running(0)
{
    //printf("[CurlmManager] Constructing the Curlm Manager!\n");

    pthread_mutex_init(&pending_lock, NULL);
    pthread_cond_init(&pending_cond, NULL);

    pthread_mutex_init(&running_lock, NULL);
    pthread_mutex_init(&ongoing_lock, NULL);
    pthread_mutex_init(&transfer_lock, NULL);
//    pthread_mutex_init(&incomplete_cache_lock, NULL);
//    pthread_mutex_init(&waiting_lock, NULL);

    pthread_mutex_init(&cache_lock, NULL);
    pthread_cond_init(&cache_cond, NULL);

    curl_global_init(CURL_GLOBAL_ALL);
    curlm = curl_multi_init();
    curl_multi_setopt(curlm, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    curl_multi_setopt(curlm, CURLMOPT_PUSHFUNCTION, CurlmManager::pushCallback);
    curl_multi_setopt(curlm, CURLMOPT_PUSHDATA, (void *)this);
}



CurlmManager::~CurlmManager()
{
    if(transfer > 0){
        /* try joining the fetching thread */
        pthread_join(fetchThread, NULL);
    }
    curl_multi_cleanup(curlm);
    curl_global_cleanup();

    /* destroy other vars */
    pthread_cond_destroy(&pending_cond);
    pthread_mutex_destroy(&pending_lock);

    pthread_cond_destroy(&cache_cond);
    pthread_mutex_destroy(&cache_lock);

    pthread_mutex_destroy(&running_lock);
    pthread_mutex_destroy(&ongoing_lock);
    pthread_mutex_destroy(&transfer_lock);


}

void CurlmManager::stopFetching()
{
    stopSignal = 1;
    pthread_cond_signal(&pending_cond);
}

void CurlmManager::startFetching()
{

    pthread_mutex_lock(&running_lock);
    if(running){
        pthread_mutex_unlock(&running_lock);
        return;
    }else{
        running = 1;
        pthread_mutex_unlock(&running_lock);
    }


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

    ret = pthread_create(&checkThread, &attr, checkPush, this);
    if(ret){
        cerr << "create thread error!" << endl;
        return;
    }

    stopSignal = 0;
}

int CurlmManager::pushCallback(CURL *parent, CURL *self, size_t headers_nr, struct curl_pushheaders *headers, void *usrp)
{

    /* a cache should be implemented to hold these content*/
    CurlmManager *cm = (CurlmManager *)usrp;

    string url;
    url = string(curl_pushheader_byname(headers, ":scheme")) + "://";
    url += curl_pushheader_byname(headers, ":authority");
    url += curl_pushheader_byname(headers, ":path");

    printf("[pushCallback] the arriving segment: %s\n", url.c_str());

    // deny the stream that has been commit to pending/ongoing queue
    pthread_mutex_lock(&cm->pending_lock);
    for(auto trans : cm->pending){
	if(trans.url == url){
	    printf("[pushCallback] The pushed content has already been in the ready queue! Deny it\n");
	    pthread_mutex_unlock(&cm->pending_lock);
	    return CURL_PUSH_DENY;
	}
    }
    pthread_mutex_unlock(&cm->pending_lock);

    pthread_mutex_lock(&cm->ongoing_lock);
    for(auto item : cm->ongoing){
	if(item.second.url == url){
	    printf("[pushCallback] The pushed content has already been in the ongoing queue! Deny it\n");
	    pthread_mutex_unlock(&cm->ongoing_lock);
	    return CURL_PUSH_DENY;
	}
    }
    pthread_mutex_unlock(&cm->ongoing_lock);

    //return CURL_PUSH_DENY;


    CacheEntry *entry = new CacheEntry(cm, url);
    // need a incomplete cache, and a complete
    curl_easy_setopt(self, CURLOPT_WRITEFUNCTION, CurlmManager::cacheCallback);
    curl_easy_setopt(self, CURLOPT_WRITEDATA, entry);

    pthread_mutex_lock(&cm->cache_lock);
    cm->incompleteCache.emplace(url, entry);
    pthread_mutex_unlock(&cm->cache_lock);

    char *headp;
    vector<string> hs;
    for(int i = 0; i < headers_nr; i++){
        headp = curl_pushheader_bynum(headers, i);
        hs.push_back(headp);
    }

    pthread_mutex_lock(&cm->transfer_lock);
    cm->transfer++;
    pthread_mutex_unlock(&cm->transfer_lock);

    pthread_mutex_lock(&cm->ongoing_lock);
    cm->ongoing.emplace(self, HttpTrans_t({url, hs, cacheCallback, cacheCompleteCallback, entry, self}));
    pthread_mutex_unlock(&cm->ongoing_lock);


    // (*((int *)userdata))++;
    return CURL_PUSH_OK;
}
void CurlmManager::cacheCompleteCallback(void *usrp)
{
    CacheEntry *entry = (CacheEntry *)usrp;
    CurlmManager *cm = entry->getManager();
    printf("[cacheCompleteCallback] Push stream for %s done!\n", entry->getUrl().c_str());
    // should put to the cache

    pthread_mutex_lock(&cm->cache_lock);
    cm->incompleteCache.erase(cm->incompleteCache.find(entry->getUrl()));
    size_t num;
    char *data;
    entry->getData(&data, &num);
    if(num > 0){
        cm->cache.emplace(entry->getUrl(), entry);
        entry->setTs(time(NULL));
    }
    pthread_mutex_unlock(&cm->cache_lock);


    printf("[cacheCompleteCallback] Cache Entries: \n");
    for(auto e : cm->cache){
        char *data;
        size_t num;
        e.second->getData(&data, &num);
        printf("          %s: %ld bytes\n", e.first.c_str(), num);
    }

    pthread_cond_signal(&cm->cache_cond);
//    delete entry;
}

size_t  CurlmManager::cacheCallback(void *contents, size_t size, size_t nmemb, void *usrp)
{
    size_t realsize = size * nmemb;
    //CurlmManager *cm = (CurlmManager *)usrp;
    CacheEntry *entry = (CacheEntry *)usrp;

    printf("[cacheCallback] Downloading %s...(%ld bytes received)\n", entry->getUrl().c_str(), realsize);
    entry->putData(contents, realsize);

    return realsize;
}

void CurlmManager::commitHttpTransaction(string url, vector<string> headers,
                                         size_t (*onprogress)(void *, size_t, size_t, void *),
                                         void (*oncomplete)(void *), void *usrp)
{

    if(incompleteCache.find(url) != incompleteCache.end() || cache.find(url) != cache.end()){
        printf("[CurlmManager] Found the committed transaction (%s) in the (incompleted) cache\n", url.c_str());
        pthread_mutex_lock(&cache_lock);
        waiting.push_back({url, headers, onprogress, oncomplete, usrp, NULL});
        pthread_mutex_unlock(&cache_lock);

        pthread_cond_signal(&cache_cond);
        return;
    }

    printf("[CurlmManager] Add an HTTP transaction (%s) into the pending queue\n", url.c_str());
    for(auto h : headers)
        printf("               %s\n", h.c_str());

    pthread_mutex_lock(&pending_lock);
    pending.push_back({url, headers, onprogress, oncomplete, usrp, NULL});
    pthread_mutex_unlock(&pending_lock);
    pthread_cond_signal(&pending_cond);
}

void CurlmManager::setupEasyHandle(CURL *easy, string url, vector<string> headers,
                                   size_t (*onprogress)(void *, size_t, size_t, void *), void *usrp)
{

    struct curl_slist *curl_headers = NULL;
    for(auto h : headers)
        curl_headers = curl_slist_append(curl_headers, h.c_str());
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, curl_headers);

    curl_easy_setopt(easy, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(easy, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, usrp);
    if(onprogress != NULL){
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, onprogress);
    }

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

void *CurlmManager::checkPush(void *data)
{
    CurlmManager *cm = (CurlmManager *)data;

    HttpTrans_t trans;
    CacheEntry *entry;
RECHECK:
    pthread_mutex_lock(&cm->cache_lock);
    while(1){
        for(auto transIt = cm->waiting.begin(); transIt != cm->waiting.end(); transIt++){
            for(auto cacheIt = cm->cache.begin(); cacheIt != cm->cache.end(); cacheIt++){
                if( (*transIt).url == (*cacheIt).first ){
		    printf("[checkPush] Find a match between the waiting requests and the cached item (%s)\n", (*transIt).url.c_str());
                    trans = *transIt;
                    entry = (*cacheIt).second;
                    cm->waiting.erase(transIt);
                    cm->cache.erase(cacheIt);
                    pthread_mutex_unlock(&cm->cache_lock);
                    goto CALLBACK;
                }
            }
        }

#define CACHE_VALID_DUR (10 * 60) // 10 min
	// Eliminate all invalid cache entries
	for(auto cacheIt = cm->cache.begin(); cacheIt != cm->cache.end(); cacheIt++){
	    entry = (*cacheIt).second;
	    if(time(NULL) - entry->getTs() > CACHE_VALID_DUR){
		printf("[checkPush] the entry of %s has exceeded the valid duration %d(%ld)\n",
		       entry->getUrl().c_str(), CACHE_VALID_DUR, (time(NULL) - entry->getTs()));
		cm->cache.erase(cacheIt);
	    }
	}

        printf("[checkPush] No matching url found, sleep...\n");
        pthread_cond_wait(&cm->cache_cond, &cm->cache_lock);
        printf("[checkPush] Wake up!\n");

    } // end while

CALLBACK:
    char *content;
    size_t num;
    entry->getData(&content, &num);
    trans.onprogress(content, 1, num, trans.usrp);
    trans.oncomplete(trans.usrp);

    delete entry;
    goto RECHECK;

    pthread_exit(0);
}

void *CurlmManager::fetchUrls(void *data)
{
    CurlmManager *cm = (CurlmManager *)data;

RESELECT:

    pthread_mutex_lock(&cm->pending_lock);
    // detect if there is any pending curl requests
    if(cm->pending.size() > 0){
        for(auto trans : cm->pending){
            printf("[fetchThread] Start fetching a url %s\n", trans.url.c_str());

            CURL *easy = curl_easy_init();
            setupEasyHandle(easy, trans.url, trans.headers, trans.onprogress, trans.usrp);
            trans.curl = easy;

            pthread_mutex_lock(&cm->transfer_lock);
            cm->transfer++;
            pthread_mutex_unlock(&cm->transfer_lock);
            curl_multi_add_handle(cm->curlm, trans.curl);

            pthread_mutex_lock(&cm->ongoing_lock);
            cm->ongoing.emplace(trans.curl, trans);
            pthread_mutex_unlock(&cm->ongoing_lock);
        }
        printf("[fetchThread] Clean up the pending queue\n");
        cm->pending.clear();
    }
    pthread_mutex_unlock(&cm->pending_lock);

    int transferrings;
    //curl_multi_perform(cm->curlm, &transferrings);

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
            printf("[fetchThread] The complete curl handle is %p\n", e);
            pthread_mutex_lock(&cm->transfer_lock);
            cm->transfer--;
            pthread_mutex_unlock(&cm->transfer_lock);

            auto trans = cm->ongoing.find(e)->second;
            trans.oncomplete(trans.usrp);
            cm->done.emplace(e, trans);

            pthread_mutex_lock(&cm->ongoing_lock);
            cm->ongoing.erase(cm->ongoing.find(e));
            pthread_mutex_unlock(&cm->ongoing_lock);

            curl_multi_remove_handle(cm->curlm, e);
            curl_easy_cleanup(e);
            printf("[fetchThread] One transfer done! (%d remain)\n", cm->transfer);
        }
    }while(m);

    if(cm->transfer > 0 || cm->pending.size() > 0)
        goto RESELECT;

    /* sleep on the pending queue and have lock on it*/
    pthread_mutex_lock(&cm->pending_lock);
    while(cm->pending.size() == 0 && !cm->stopSignal){
        printf("[fetchThread] No pending transfer request! Sleep...\n");
        pthread_cond_wait(&cm->pending_cond, &cm->pending_lock);
        printf("[fetchThread] Wake up~\n");
    }
    pthread_mutex_unlock(&cm->pending_lock);

    printf("[fetchThread] We got %ld new http requests~\n", cm->pending.size());
    if(!cm->stopSignal)
        goto RESELECT;

    printf("[fetchThread] Receive stop signal, fetching thread terminates.\n");
FETCHING_END:
    cm->running = 0;
    pthread_exit(0);
}


/*
 * Cache Entry Methods
 */


CacheEntry::CacheEntry(CurlmManager *cm, string u) : url(u), data(NULL), size(0), manager(cm)
{
    this->ts = time(NULL);
}
CacheEntry::~CacheEntry()
{
    if (this->data != NULL)
        free(this->data);
}

size_t CacheEntry::putData(void *src, size_t src_s)
{
    if(src_s > 0){
        data = (char *)realloc(data, size + src_s);
        memcpy(data + size, src, src_s);
        size += src_s;
    }

    return src_s;
}
void CacheEntry::getData(char **d, size_t *s)
{
    *d = this->data;
    *s = this->size;
}
void CacheEntry::appendHeaders(const char *header)
{
    this->headers.push_back(string(header));
}
string &CacheEntry::getUrl()
{
    return this->url;
}

CurlmManager *CacheEntry::getManager()
{
    return this->manager;
}


time_t CacheEntry::getTs()
{
    return this->ts;
}

void CacheEntry::setTs(time_t t)
{
    this->ts = t;
}
