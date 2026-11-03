#include "track_info.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){printf("FAIL line=%d\n",__LINE__);return 1;}}while(0)
int main(){
    std::vector<CloudTrack> tracks={{1,L"第一首",L"甲"},{2,L"たぶん",L"YOASOBI"}};
    CloudStatus actual{L"第一首",L"甲",4,true};
    auto display=describe_cloud(actual,tracks,1,tracks[1],true,false);
    CHECK(display.pending&&display.card.title==L"第一首"&&display.card.state==L"切歌待确认");
    CHECK(display.card.detail.find(L"たぶん")!=std::wstring::npos);
    display=describe_cloud(actual,tracks,1,tracks[1],true,true);
    CHECK(display.pending&&display.card.title==L"第一首"&&display.card.state==L"切歌未确认");
    actual={L"たぶん",L"YOASOBI",5,true};
    display=describe_cloud(actual,tracks,0,tracks[1],true,false);
    CHECK(!display.pending&&display.index==1&&display.card.state==L"已暂停");
    actual.title=L"\u305f\u3075\u3099\u3093"; // NFD dakuten from public metadata.
    display=describe_cloud(actual,tracks,0,tracks[1],true,false);
    CHECK(!display.pending&&display.index==1);
    actual={L"第一首",L"甲",4,true};
    display=describe_cloud(actual,tracks,1,tracks[1],false,false);
    CHECK(display.index==0&&display.card.detail.find(L"1 / 2")!=std::wstring::npos);
    actual={L"客户端自己播放的歌",L"乙",4,true};
    display=describe_cloud(actual,tracks,1,tracks[1],false,false);
    CHECK(display.card.title==actual.title&&display.card.detail.find(L"客户端队列")!=std::wstring::npos);
    display=describe_cloud(CloudStatus{},tracks,1,tracks[1],true,false);
    CHECK(display.pending&&display.card.title!=tracks[1].title&&display.card.artist.empty());
    actual={L"たぶん",L"其他歌手",4,true};
    CHECK(describe_cloud(actual,tracks,1,tracks[1],true,false).pending);
    puts("PASS: actual/requested distinction, timeout, confirmation, NFC/NFD titles, pause state, natural next, foreign queue, missing SMTC, same-title artist guard");return 0;
}
