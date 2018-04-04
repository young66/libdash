#include <iostream>
#include "CurlmManager.h"

#include <vector>
#include <string>
#include "curl/curl.h"
#include <unistd.h>
#include "pthread.h"
#include <stdlib.h>
#include <cstring>

using namespace std;
using namespace xmb;


#define THREAD_NR 20

#include <cstdio>


static pthread_mutex_t id_lock;
static int id = 1;


//int setup_handle_kpush(CURL *handle, const char *url, FILE *fp, int k)
int setup_handle_kpush(CURL *handle, const char *url, FILE *fp)
{
    // char pushdirective[256];
    // struct curl_slist *headers = NULL;

    // memset(pushdirective, '\0', 256);
    // snprintf(pushdirective, 256, "PushDirective: urn:mpeg:dash:fdh:2016:push-next;%d", k);

    // headers = curl_slist_append(headers, pushdirective);

    // curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(handle, CURLOPT_VERBOSE, 0L);

    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);

    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(handle, CURLOPT_PIPEWAIT, 1L);

    curl_easy_setopt(handle, CURLOPT_URL, url);

    curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp);

    return 0;
}

static void showResult(void *usrp)
{
    FILE *fp = (FILE *)usrp;
    fflush(fp);
    fclose(fp);
}

static void *createCurlCommit(void *usrp)
{
    CurlmManager *cm = (CurlmManager *)usrp;

//    const int end = 20;
    // char seg_name[256];
    // const char* path = "https://localhost:8444/vod/";
    // char url[256];


    // memset(url, '\0', 256);
    // memset(seg_name, '\0', 256);

    string path = "https://localhost:8444/vod/";
    string seg_name;
    string url;

    pthread_mutex_lock(&id_lock);
    seg_name = "counter-4-" + std::to_string(id++) + ".m4s";
//    snprintf(seg_name, 256, "counter-4-%d.m4s", id++);
    pthread_mutex_unlock(&id_lock);

    printf("[menxiao] set output to %s\n", seg_name.c_str());
    FILE *fp = fopen(seg_name.c_str(), "wb");

    // strcpy(url, path);
    // strcat(url, seg_name);
    url = path + seg_name;

    printf("[menxiao] the url is %s\n", url.c_str());

    cm->commitHttpTransaction(url, NULL, showResult, fp);
    // CURL *easy = curl_easy_init();
    // setup_handle_kpush(easy, url, fp);

    // cm->commitEasyHandle(easy);

    pthread_exit(0);
}


int main(int argc, char* argv[])
{
    cout << "Hello World!" << endl;

    pthread_mutex_init(&id_lock, NULL);

    CurlmManager cmanager = CurlmManager();
//    CURL *curls[10];

    cmanager.startFetching();

    pthread_t threads[THREAD_NR];
    for(int i = 0; i < THREAD_NR/2; i++){
        pthread_create(&threads[i], NULL, createCurlCommit, &cmanager);
    }

    // for(auto &curl : curls) {
    //     cmanager.addEasyHandle(curl);
    // }
    sleep(2);

    for(int i = THREAD_NR/2+1; i < THREAD_NR; i++){
        pthread_create(&threads[i], NULL, createCurlCommit, &cmanager);
    }


    fgetc(stdin);
    cmanager.stopFetching();
    pthread_mutex_destroy(&id_lock);
    return 0;
}
