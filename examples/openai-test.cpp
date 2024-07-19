
#include <iostream>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <vector>
 
using std::string;
using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using nlohmann::json;

using std::vector;
 


string apiKey = "sk-on-Jo5FAGExsug1vrAvXCQ";       //修改为自己的api Key,baseurl 
string baseUrl = "https://agino.me/chat/completions";
string thisModel = "gpt-3.5-turbo"; 




//聊天记录 
struct msgRec
{
    string role;          //角色
    string content;        //内容

};

vector<msgRec> msg_list;    //保留聊天记录，用于上下文关联聊天
 


size_t WriteCallback(void*, size_t, size_t, string*);
string getCompletion(const string& prompt);


int main() {
    
    string prompt;
    
    cout << "\n====请输入要提问的内容, new开始新话题，或quit退出====\n" << endl;
 
     while (true)
      	{
          cout << "\nEnter a question>>>>>>";
          getline(cin, prompt);
          if (prompt.substr(0,4)=="quit")
          {
                break;
          }
          else if  (prompt.substr(0,3)=="new")    //开始新话题
          {
                msg_list.clear();
          } 
          else
          {

            if (msg_list.size()>4)     //只保留2次聊天记录，减少token消耗
            {
                  msg_list.erase(msg_list.begin());
                  msg_list.erase(msg_list.begin());
            }
                      
            msgRec rec;
            rec.role="user";
            rec.content=prompt;
            msg_list.push_back(rec);

            
            json requestData;
            requestData["model"] = thisModel;
            
            
            for (int i = 0; i < msg_list.size(); ++i) {

                requestData["messages"][i]["role"] = msg_list[i].role;
                requestData["messages"][i]["content"] = msg_list[i].content;
            }
    
            

            
            requestData["temperature"] = 0;
     
            string requestDataStr = requestData.dump();
            
           // cout << "requestDataStr======"<<requestDataStr << endl;
            
            string response = getCompletion(requestDataStr);
            
            
            rec.role="assistant";
            rec.content=response;
            msg_list.push_back(rec);
            
            
            cout << "\n<<<<<<"<<response << endl;

          }
      	} 
 

 
    return 0;
}
 
// Handle data received from the server
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}
 
//
string getCompletion(const string& requestDataStr) {

    

    
            
    
    
    string response;
    CURL* curl = curl_easy_init();
 
    if (curl) {

 
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, baseUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestDataStr.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestDataStr.length());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
 
        if (res != CURLE_OK) {
            cerr << "Curl request failed: " << curl_easy_strerror(res) << endl;
        }
 
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
 
    // return response;
    json jresponse = json::parse(response);
    return jresponse["choices"][0]["message"]["content"].get<string>();
}
