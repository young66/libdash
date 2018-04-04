/*
 * MediaObject.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "MediaObject.h"
#include <stdio.h>

using namespace libdash::framework::input;

using namespace dash::mpd;
using namespace dash::network;
using namespace dash::metrics;

MediaObject::MediaObject    (ISegment *segment, IRepresentation *rep, int k) :
             segment        (segment),
             rep            (rep),
             k              (k)
{
    InitializeConditionVariable (&this->stateChanged);
    InitializeCriticalSection   (&this->stateLock);
}
MediaObject::~MediaObject   ()
{
    if(this->state == IN_PROGRESS)
    {
        this->segment->AbortDownload();
        this->OnDownloadStateChanged(ABORTED);
    }
    this->segment->DetachDownloadObserver(this);
    this->WaitFinished();

    DeleteConditionVariable (&this->stateChanged);
    DeleteCriticalSection   (&this->stateLock);
}

bool                MediaObject::StartDownload          ()
{
    this->segment->AttachDownloadObserver(this);

    if(k > 0){
        char pushDirective[100];
        sprintf(pushDirective, "PushDirective: urn:mpeg:dash:fdh:2016:push-next;%d", this->k);
        std::vector<std::string> headers = { pushDirective };

        return this->segment->StartDownload(headers);
    }else{
        return this->segment->StartDownload();
    }
}
void                MediaObject::AbortDownload          ()
{
    this->segment->AbortDownload();
    this->OnDownloadStateChanged(ABORTED);
}
void                MediaObject::WaitFinished           ()
{
    EnterCriticalSection(&this->stateLock);

    while(this->state != COMPLETED && this->state != ABORTED)
        SleepConditionVariableCS(&this->stateChanged, &this->stateLock, INFINITE);

    LeaveCriticalSection(&this->stateLock);
}
int                 MediaObject::Read                   (uint8_t *data, size_t len)
{
    return this->segment->Read(data, len);
}
int                 MediaObject::Peek                   (uint8_t *data, size_t len)
{
    return this->segment->Peek(data, len);
}
int                 MediaObject::Peek                   (uint8_t *data, size_t len, size_t offset)
{
    return this->segment->Peek(data, len, offset);
}
IRepresentation*    MediaObject::GetRepresentation      ()
{
    return this->rep;
}
void                MediaObject::OnDownloadStateChanged (DownloadState state)
{
    EnterCriticalSection(&this->stateLock);

    this->state = state;

    WakeAllConditionVariable(&this->stateChanged);
    LeaveCriticalSection(&this->stateLock);
}
void                MediaObject::OnDownloadRateChanged  (uint64_t bytesDownloaded)
{
}
const std::vector<ITCPConnection *>&    MediaObject::GetTCPConnectionList   () const
{
    return this->segment->GetTCPConnectionList();
}
const std::vector<IHTTPTransaction *>&  MediaObject::GetHTTPTransactionList () const
{
    return this->segment->GetHTTPTransactionList();
}
