/*
 * DASHManager.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHManager.h"

#include <cstdio>
#include <cstring>
#include "curl/curl.h"

using namespace dash;
using namespace dash::xml;
using namespace dash::mpd;
using namespace dash::network;
using namespace dash::helpers;

DASHManager::DASHManager            ()
{
}
DASHManager::~DASHManager           ()
{
}
IMPD*           DASHManager::Open   (char *path)
{
    DOMParser parser(path);

    // get the file name
    const char *filename = strrchr(path, '/') + 1;
    FILE *fp = fopen(filename, "wb");

    printf("[menxiao] Try retrieving file from url %s, and dumping to %s\n", path, filename);

    CURL *curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    //curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, path);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    curl_easy_perform(curl);
    fflush(fp);
    fclose(fp);
    curl_easy_cleanup(curl);
    uint32_t fetchTime = Time::GetCurrentUTCTimeInSec();

    if (!parser.Parse(filename)){
        printf("[menxiao] parser.Parse() failed\n");
        return NULL;
    }

    MPD* mpd = parser.GetRootNode()->ToMPD();

    if (mpd)
        mpd->SetFetchTime(fetchTime);

    return mpd;
}
void            DASHManager::Delete ()
{
    delete this;
}
