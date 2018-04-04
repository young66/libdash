#ifndef CURLMMANAGER_H_
#define CURLMMANAGER_H_

#include "config.h"
#include "../../../libcurl/include/curl/curl.h"
#include <pthread.h>
#include "stdlib.h"
#include "time.h"

namespace dash
{
    namespace network
    {
        class CurlmManager;

        typedef struct  _HttpTrans {
            std::string url;
            std::vector<std::string> headers;
            size_t (*onprogress)(void *, size_t, size_t, void *);
            void (*oncomplete)(void *);
            void *usrp;
            CURL *curl;
        } HttpTrans_t;

        class CacheEntry {
        public :
            CacheEntry(CurlmManager *cm, std::string u);
            virtual ~CacheEntry();

            size_t putData(void *src, size_t src_s);
            void getData(char **d, size_t *s);
            void appendHeaders(const char *header);
            std::string &getUrl();
            CurlmManager *getManager();

            time_t getTs();
            void setTs(time_t t);
        private:
            std::string url;
            std::vector<std::string> headers;
            size_t size;
            char *data;
            CurlmManager *manager;
            time_t ts;
        };

        class CurlmManager {
        public:
            CurlmManager();
            virtual ~CurlmManager();

            //void commitEasyHandle(CURL *easy);
            int checkEasyHandleDoneOrFailed(CURL *easy);
            void commitHttpTransaction(std::string url, std::vector<std::string> headers, size_t (*onprogress)(void *, size_t, size_t, void *), void (*oncomplete)(void *), void *usrp);


            void stopFetching();
            void startFetching();
        private:
            CURLM *curlm;
            int transfer;
            pthread_mutex_t transfer_lock;

            std::vector<HttpTrans_t> pending;
            pthread_mutex_t pending_lock;
            pthread_cond_t pending_cond;

            std::map<CURL *, HttpTrans_t> ongoing;
            pthread_mutex_t ongoing_lock;

            std::map<CURL *, HttpTrans_t> done;
            std::map<CURL *, HttpTrans_t> failed;

            pthread_t fetchThread;
            int stopSignal;
            static void *fetchUrls(void *data);

            std::map<std::string, CacheEntry *> incompleteCache;
//            pthread_mutex_t incomplete_cache_lock;

            std::vector<HttpTrans_t> waiting;
//            pthread_mutex_t waiting_lock;

            std::map<std::string, CacheEntry *> cache;
            pthread_mutex_t cache_lock;
            pthread_cond_t cache_cond;

            pthread_t checkThread;
            static void *checkPush(void *data);

            pthread_mutex_t running_lock;
            int running;

            static void setupEasyHandle(CURL *easy, std::string url, std::vector<std::string> headers, size_t (*onprogress)(void *, size_t, size_t, void *), void *usrp);

            static int pushCallback(CURL *parent, CURL *self, size_t headers_nr, struct curl_pushheaders *headers, void *usrp);
            static size_t  cacheCallback(void *contents, size_t size, size_t nmemb, void *usrp);
            static void cacheCompleteCallback(void *usrp);

        };
    }
}

#endif
