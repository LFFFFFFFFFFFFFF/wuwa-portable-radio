#pragma once
#include "cloud.h"
#include "status.h"
#include <vector>
#include <utility>

inline std::wstring canonical_text(const std::wstring &value) {
    if(value.empty()||value.size()>32767)return value;
    int size=NormalizeString(NormalizationC,value.data(),(int)value.size(),nullptr,0);
    if(size<=0)return value;
    std::wstring result((size_t)size,L'\0');
    size=NormalizeString(NormalizationC,value.data(),(int)value.size(),result.data(),size);
    if(size<=0)return value;
    result.resize((size_t)size);return result;
}
struct CloudTrack {
    uint64_t id;std::wstring title,artist,key_title,key_artist;
    CloudTrack(uint64_t value=0,std::wstring name={},std::wstring singer={}):id(value),title(std::move(name)),
        artist(std::move(singer)),key_title(canonical_text(title)),key_artist(canonical_text(artist)){}
};
struct CloudDisplay {RadioStatus card;size_t index;bool pending;};
inline bool same_track(const CloudStatus &normalized,const CloudTrack &track) {
    return normalized.available&&!track.key_title.empty()&&normalized.title==track.key_title&&
        (track.key_artist.empty()||normalized.artist==track.key_artist);
}
inline CloudDisplay describe_cloud(const CloudStatus &actual,const std::vector<CloudTrack> &tracks,
                                  size_t index,const CloudTrack &requested,bool pending,bool timed_out) {
    CloudStatus normalized=actual;
    normalized.title=canonical_text(actual.title);normalized.artist=canonical_text(actual.artist);
    CloudDisplay result{{},index,pending&&!same_track(normalized,requested)};
    auto &card=result.card;
    card.source=L"网易云";
    card.title=actual.available&&!actual.title.empty()?actual.title:L"等待网易云曲目信息";
    card.artist=actual.artist;
    card.state=!actual.available?L"未连接 SMTC":actual.playback==4?L"播放中":actual.playback==5?L"已暂停":L"未播放";
    if(result.pending){
        card.state=timed_out?L"切歌未确认":L"切歌待确认";
        card.detail=(timed_out?L"未确认切到：":L"正在切到：")+
            (requested.title.empty()?std::to_wstring(requested.id):requested.title);
    }else {
        size_t found=tracks.size();
        for(size_t i=0;i<tracks.size();++i)if(same_track(normalized,tracks[i])){found=i;break;}
        if(found<tracks.size()){
            result.index=found;
            card.detail=L"导入列表 "+std::to_wstring(found+1)+L" / "+std::to_wstring(tracks.size())+L" · F10 切换";
        }else card.detail=L"网易云客户端队列 · F10 切回导入列表";
        if(!actual.available)card.detail=L"请保持网易云打开，并在设置中开启 SMTC";
    }
    return result;
}
