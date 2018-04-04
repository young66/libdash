#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include "curl/curl.h"

static int push_callback(CURL *parent,
                         CURL *self,
                         size_t headers_nr,
                         struct curl_pushheaders *headers,
                         void *userdata)
{
    (void)parent;

    char* headp;
    headp = curl_pushheader_byname(headers, ":path");
    const char* filename = strrchr(headp, '/') + 1;

    printf("[menxiao] push stream dump to file %s\n", filename);

    FILE *fp = fopen(filename, "wb");

    curl_easy_setopt(self, CURLOPT_WRITEDATA, fp);

    /* int i; */
    /* for(i = 0; i < headers_nr; i++){ */
    /*     headp = curl_pushheader_bynum(headers, i); */
    /*     printf("[headers] %s\n", headp); */
    /* } */

    (*((int *)userdata))++;
    return CURL_PUSH_OK;
}

int setup_handle_kpush(CURL *handle,
                       const char *url,
                       FILE *fp,
                       int k)
{
    char pushdirective[256];
    struct curl_slist *headers = NULL;

    memset(pushdirective, '\0', 256);
    snprintf(pushdirective, 256, "PushDirective: urn:mpeg:dash:fdh:2016:push-next;%d", k);

    headers = curl_slist_append(headers, pushdirective);

    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(handle, CURLOPT_VERBOSE, 0L);

    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);

    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(handle, CURLOPT_PIPEWAIT, 1L);

    curl_easy_setopt(handle, CURLOPT_URL, url);

    curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp);
}


int main(int argc, char* argv[])
{

    int transfer = 0;
    CURLM *handles = curl_multi_init();
    CURL *handle; // = curl_easy_init();


    const char *mpd_url = "https://localhost:8444/vod/counter.mpd";
    const char *mpd = "counter.mpd";
    FILE *mpd_fp = fopen(mpd, "wb");

    printf("[menxiao] Try retrieving file from url %s, and dumping to %s\n", mpd_url, mpd);

    CURL *mpd_curl = curl_easy_init();

    curl_easy_setopt(mpd_curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(mpd_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    // curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(mpd_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(mpd_curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(mpd_curl, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(mpd_curl, CURLOPT_URL, mpd_url);

    curl_easy_setopt(mpd_curl, CURLOPT_WRITEDATA, mpd_fp);

    curl_easy_perform(mpd_curl);
    fflush(mpd_fp);
    fclose(mpd_fp);
    curl_easy_cleanup(mpd_curl);

    // setup for http2 push
    curl_multi_setopt(handles, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    // push_callback(CURL *parent, CURL *self, size_t num_headers, struct curl_pushheaders *headers, void *userdata)
    // return CURL_PUSH_OK or CURL_PUSH_DENY
    curl_multi_setopt(handles, CURLMOPT_PUSHFUNCTION, push_callback);
    curl_multi_setopt(handles, CURLMOPT_PUSHDATA, &transfer); // user data pointer passed into push_callback()

/*
    // may setup handle here
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "PushDirective:  urn:mpeg:dash:fdh:2016:push-next;4");


    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    // curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, func) may be required to provide flexible write operation

    curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    // check how to implement the debug function
    //curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, debug_func);

    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    // all sessions use one connection
    curl_easy_setopt(handle, CURLOPT_PIPEWAIT, 1L);
*/

    int progress = 0;
    const int end = 20;
    char seg_name[256];
    const char* path = "https://localhost:8444/vod/";
    char url[256];

NEW_TURN:

    printf("[menxiao] current progress is %d\n", progress);
    memset(url, '\0', 256);
    memset(seg_name, '\0', 256);

    snprintf(seg_name, 256, "counter-1-%d.m4s", progress + 1);
    printf("[menxiao] set output to %s\n", seg_name);
    FILE *fp = fopen(seg_name, "wb");

    strcpy(url, path);
    strcat(url, seg_name);

    printf("[menxiao] the url is %s\n", url);

    handle = curl_easy_init();
    setup_handle_kpush(handle, url, fp, 4);
    transfer++;
/*
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp);
*/

    // add handle into handles
    curl_multi_add_handle(handles, handle);




    int runnings;
    curl_multi_perform(handles, &runnings);
    printf("1. running task #: %d\n", runnings);

    struct timeval timeout;
    long curlto;

    fd_set fdread, fdwrite, fdexcep;
    int maxfd;
RESELECT:
    maxfd = -1;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    curl_multi_timeout(handles, &curlto);
    if(curlto >= 0){
        timeout.tv_sec = curlto / 1000;
        timeout.tv_usec = (curlto % 1000) * 1000;
    }else{
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }

    CURLMcode mc;
    mc = curl_multi_fdset(handles, &fdread, &fdwrite, &fdexcep, &maxfd);
    if(mc != CURLM_OK){
        printf("multi fdset failed!\n");
        exit(1);
    }

    int rc;
    if(maxfd == -1){
        printf("[menxiao] failed to set fdset, wait for 100ms\n");
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
        curl_multi_perform(handles, &runnings);
        printf("2. running task #: %d\n", runnings);
        break;
    }

    struct CURLMsg *m;
    // use the message to figure out when the push stream is done
    do{
        int msgq = 0;
        m = curl_multi_info_read(handles, &msgq);
        if(m && (m->msg == CURLMSG_DONE)) {
            printf("get a message! the concern curl is %p\n", m->easy_handle);
            CURL *e = m->easy_handle;
            transfer--;
            progress++;
            curl_multi_remove_handle(handles, e);
            curl_easy_cleanup(e);
        }
    }while(m);

    if(transfer > 0)
        goto RESELECT;


    if(progress < end)
        goto NEW_TURN;

    curl_multi_cleanup(handles);

    return 0;
}
