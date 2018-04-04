#ifndef _M_CURLMMANAGER_H
#define _M_CURLMMANAGER_H

#include "curl/curl.h"
#include <string>
#include <map>
#include <vector>
#include <pthread.h>

namespace xmb
{
    typedef struct  _HttpTrans {
        std::string url;
        size_t (*onprogress)(void *, size_t, size_t, void *);
        void (*oncomplete)(void *);
        void *usrp;
        CURL *curl;
    } HttpTrans_t;


    class CurlmManager {
    public:
        CurlmManager();
        virtual ~CurlmManager();

        //void commitEasyHandle(CURL *easy);
        int checkEasyHandleDoneOrFailed(CURL *easy);
        void commitHttpTransaction(std::string url, size_t (*onprogress)(void *, size_t, size_t, void *), void (*oncomplete)(void *), void *usrp);


        void stopFetching();
        void startFetching();
    private:
        CURLM *curlm;
        int transfer;

//        std::vector<CURL *> pending;
        std::vector<HttpTrans_t> pending;
        pthread_mutex_t pending_lock;
        pthread_cond_t pending_cond;

        std::map<CURL *, HttpTrans_t> ongoing;
        std::map<CURL *, HttpTrans_t> done;
        std::map<CURL *, HttpTrans_t> failed;

        pthread_t fetchThread;
        int stopSignal;
        int running;

        //pthread_mutex_t

        static void *fetchUrls(void *data);
        static int pushCallback(CURL *parent, CURL *self, size_t headers_nr, struct curl_pushheaders *headers, void *userdata);

    };
}

#endif
