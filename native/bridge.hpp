#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>
#undef near
#undef far
#include <thread>
#include <mutex>
#include <deque>
#include <string>
#include <atomic>
#include <filesystem>
#include "vendor/json.hpp"
using json=nlohmann::json;

class RulesBridge {
 HANDLE input=nullptr,output=nullptr,process=nullptr;
 std::thread reader;
 std::mutex mutex;
 std::deque<json> incoming;
 std::atomic<bool> alive{false};
 int sequence=0;
public:
 bool start(const std::filesystem::path& directory) {
  SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
  HANDLE childIn=nullptr,childOut=nullptr;
  if(!CreatePipe(&childIn,&input,&sa,0)||!CreatePipe(&output,&childOut,&sa,0))return false;
  SetHandleInformation(input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(output,HANDLE_FLAG_INHERIT,0);
  auto executable=directory/L"runtime"/L"node.exe";auto script=directory/L"rules.cjs";
  std::wstring command=L"\""+executable.wstring()+L"\" \""+script.wstring()+L"\"";
  STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=childIn;si.hStdOutput=childOut;
  HANDLE errorFile=CreateFileW((directory/L"rules-errors.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);si.hStdError=errorFile;
  PROCESS_INFORMATION pi{};
  BOOL ok=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,directory.c_str(),&si,&pi);
  CloseHandle(childIn);CloseHandle(childOut);if(errorFile!=INVALID_HANDLE_VALUE)CloseHandle(errorFile);
  if(!ok)return false;CloseHandle(pi.hThread);process=pi.hProcess;alive=true;
  reader=std::thread([this]{std::string pending;char buffer[65536];DWORD read=0;while(ReadFile(output,buffer,sizeof(buffer),&read,nullptr)&&read){pending.append(buffer,read);size_t pos;while((pos=pending.find('\n'))!=std::string::npos){try{auto value=json::parse(pending.substr(0,pos));std::lock_guard<std::mutex> lock(mutex);incoming.push_back(std::move(value));}catch(...){}pending.erase(0,pos+1);}}alive=false;});
  return true;
 }
 bool running()const{return alive;}
 int send(json message){if(!alive)return -1;message["id"]=++sequence;std::string data=message.dump()+"\n";DWORD written=0;size_t total=0;while(total<data.size()){if(!WriteFile(input,data.data()+total,(DWORD)(data.size()-total),&written,nullptr))return -1;total+=written;}return sequence;}
 bool poll(json& message){std::lock_guard<std::mutex> lock(mutex);if(incoming.empty())return false;message=std::move(incoming.front());incoming.pop_front();return true;}
 ~RulesBridge(){if(input)CloseHandle(input);if(process){if(WaitForSingleObject(process,1500)==WAIT_TIMEOUT)TerminateProcess(process,0);CloseHandle(process);}if(reader.joinable())reader.join();if(output)CloseHandle(output);}
};
