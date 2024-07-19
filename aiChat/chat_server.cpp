#include <ctime>
#include <string>
#include <deque>
#include <iostream>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iomanip>
#include <array>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "protocol.hpp"


#include <curl/curl.h>
#include <nlohmann/json.hpp>




using boost::asio::ip::tcp;
using namespace std;
using nlohmann::json;

namespace
{



//string apiKey = "sk-QJt-RF2Jumuscl2RSsozPg";       
//string baseUrl = "https://agino.me/chat/completions";


string apiKey = "sk-on-Jo5FAGExsug1vrAvXCQ";       //修改为自己的api Key,baseurl 
string baseUrl = "https://agino.me/chat/completions";

string thisModel = "gpt-3.5-turbo"; 

//和openai的聊天记录 
struct msgRec
{
    string role;          //角色
    string content;        //内容

};

deque<msgRec> msg_list;    //保留和openai的聊天记录，用于上下文关联聊天




 
// 从服务器接收数据
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}
 
//
std::string getCompletion(const string& requestDataStr) {

    string response;
    CURL* curl = curl_easy_init();
    
    try {
 
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
     catch (const std::exception& e)
       {

            string errMsg=e.what();
            errMsg="***获取OpenAi回复时出错,通常是网络连接问题,请稍后重试: " +errMsg;
            
           // std::cerr <<errMsg<<endl;
            
            return errMsg;
       }
    
    
}


std::string getTimestamp()
{
    time_t t = time(0);   // get time now
    struct tm * now = localtime(&t);
    std::stringstream ss;
    ss << '[' << (now->tm_year + 1900) << '-' << std::setfill('0')
       << std::setw(2) << (now->tm_mon + 1) << '-' << std::setfill('0')
       << std::setw(2) << now->tm_mday << ' ' << std::setfill('0')
       << std::setw(2) << now->tm_hour << ":" << std::setfill('0')
       << std::setw(2) << now->tm_min << ":" << std::setfill('0')
       << std::setw(2) << now->tm_sec << "] ";

    return ss.str();
}

class workerThread
{
public:
    static void run(std::shared_ptr<boost::asio::io_service> io_service)
    {
        {
            std::lock_guard < std::mutex > lock(m);
           // std::cout << "[" << std::this_thread::get_id() << "] Thread starts" << std::endl;
        }

        io_service->run();

        {
            std::lock_guard < std::mutex > lock(m);
           // std::cout << "[" << std::this_thread::get_id() << "] Thread ends" << std::endl;
        }

    }
private:
    static std::mutex m;
};

std::mutex workerThread::m;
}

class participant
{
public:
    virtual ~participant() {}
    virtual void onMessage(std::array<char, MAX_IP_PACK_SIZE> & msg) = 0;
};


class chatRoom {
public:
    void enter(std::shared_ptr<participant> participant, const std::string & nickname)
    {
        //printf("%s ------ %s 进入聊天室\n",getTimestamp().c_str(),nickname.c_str());
        
        std::cout<<getTimestamp()<<"------"<<nickname.substr(0, nickname.length() - 2)<<" 进入聊天室"<<std::endl;
        
        participants_.insert(participant);
        name_table_[participant] = nickname;
        std::for_each(recent_msgs_.begin(), recent_msgs_.end(),
                      boost::bind(&participant::onMessage, participant, _1));
                      
       
    }

    void leave(std::shared_ptr<participant> participant)
    {
        std::string nickname= getNickname(participant); 
        std::cout<<getTimestamp()<<"------"<<nickname.substr(0, nickname.length() - 2)<<" 离开聊天室"<<std::endl;
        
        participants_.erase(participant);
        name_table_.erase(participant);
    }

    void broadcast(std::array<char, MAX_IP_PACK_SIZE>& msg, std::shared_ptr<participant> participant)
    {
        std::string timestamp = getTimestamp();
        std::string nickname = getNickname(participant);
        std::array<char, MAX_IP_PACK_SIZE> formatted_msg;

        // boundary correctness is guarded by protocol.hpp
        strcpy(formatted_msg.data(), timestamp.c_str());
        strcat(formatted_msg.data(), "------");
        strcat(formatted_msg.data(), nickname.c_str());
        strcat(formatted_msg.data(), msg.data());
        


        recent_msgs_.push_back(formatted_msg);


        std::for_each(participants_.begin(), participants_.end(),
                      boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
        
        
        
        if (!strncmp(msg.data(), "(new)", 5))  //开始新话题
        {
             msg_list.clear();
             
            
        }
        else
        {
            string strMsg=msg.data();
            
            //std::cout<<"=========="<<strMsg<<std::endl;
            

            
            
            if (msg_list.size()>4)     //只保留2次聊天记录，减少token消耗
            {
                  msg_list.erase(msg_list.begin());
                  msg_list.erase(msg_list.begin());
            }
                      
            msgRec rec;
            rec.role="user";
            rec.content=strMsg;
            msg_list.push_back(rec);

            
            json requestData;
            requestData["model"] = thisModel;
            
            
            for (int i = 0; i < msg_list.size(); ++i) {

                requestData["messages"][i]["role"] = msg_list[i].role;
                requestData["messages"][i]["content"] = msg_list[i].content;
            }
    
            
            requestData["max_tokens"] = 500;     //一次生成使用的最大令牌数 ，减少token消耗
            
            requestData["temperature"] = 0;
            
            
     
            string requestDataStr = requestData.dump();
            
            //std::cout << "requestDataStr======"<<requestDataStr << std::endl;
            
            string response = getCompletion(requestDataStr);
            
            
            rec.role="assistant";
            rec.content=response;
            msg_list.push_back(rec);
            
 
            
            strcpy(formatted_msg.data(), getTimestamp().c_str());
            strcat(formatted_msg.data(), "------");
            strcat(formatted_msg.data(), "OpenAi :");
            strcat(formatted_msg.data(), response.c_str());
            
            recent_msgs_.push_back(formatted_msg);
            
            std::for_each(participants_.begin(), participants_.end(),
                          boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
        }
        
        while (recent_msgs_.size() > max_recent_msgs)
        {
            recent_msgs_.pop_front();
        }
        

    }

    std::string getNickname(std::shared_ptr<participant> participant)
    {
        return name_table_[participant];
    }

private:
    enum { max_recent_msgs = 100 };
    std::unordered_set<std::shared_ptr<participant>> participants_;
    std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;
    std::deque<std::array<char, MAX_IP_PACK_SIZE>> recent_msgs_;
};



class personInRoom: public participant,
                    public std::enable_shared_from_this<personInRoom>
{
    public:
        personInRoom(boost::asio::io_service& io_service,
                     boost::asio::io_service::strand& strand, chatRoom& room)
                     : socket_(io_service), strand_(strand), room_(room)
        {
        }
    
        tcp::socket& socket() { return socket_; }
    
        void start()
        {
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(nickname_, nickname_.size()),
                                    strand_.wrap(boost::bind(&personInRoom::nicknameHandler, shared_from_this(), _1)));
    
            
        }
    
        void onMessage(std::array<char, MAX_IP_PACK_SIZE>& msg)
        {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(msg);
            
    
            
            if (!write_in_progress)
            {
                boost::asio::async_write(socket_,
                                         boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                                         strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
            }
        }
    
    private:
        void nicknameHandler(const boost::system::error_code& error)
        {
            if (strlen(nickname_.data()) <= MAX_NICKNAME - 2)
            {
                strcat(nickname_.data(), ": ");
            }
            else
            {
                //cut off nickname if too long
                nickname_[MAX_NICKNAME - 2] = ':';
                nickname_[MAX_NICKNAME - 1] = ' ';
            }
    
            room_.enter(shared_from_this(), std::string(nickname_.data()));
    
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_, read_msg_.size()),
                                    strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
        }
    
        void readHandler(const boost::system::error_code& error)
        {
            if (!error)
            {
                room_.broadcast(read_msg_, shared_from_this());
    
                boost::asio::async_read(socket_,
                                        boost::asio::buffer(read_msg_, read_msg_.size()),
                                        strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
            }
            else
            {
                room_.leave(shared_from_this());
            }
        }
    
        void writeHandler(const boost::system::error_code& error)
        {
            if (!error)
            {
                write_msgs_.pop_front();
    
                if (!write_msgs_.empty())
                {
                    boost::asio::async_write(socket_,
                                             boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                                             strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                }
            }
            else
            {
                room_.leave(shared_from_this());
            }
        }
    
        tcp::socket socket_;
        boost::asio::io_service::strand& strand_;
        chatRoom& room_;
        std::array<char, MAX_NICKNAME> nickname_;
        std::array<char, MAX_IP_PACK_SIZE> read_msg_;
        std::deque<std::array<char, MAX_IP_PACK_SIZE> > write_msgs_;
};




class server
{
    public:
        server(boost::asio::io_service& io_service,
               boost::asio::io_service::strand& strand,
               const tcp::endpoint& endpoint)
               : io_service_(io_service), strand_(strand), acceptor_(io_service, endpoint)
        {
            run();
        }
    
    private:
    
        void run()
        {
            std::shared_ptr<personInRoom> new_participant(new personInRoom(io_service_, strand_, room_));
            acceptor_.async_accept(new_participant->socket(), strand_.wrap(boost::bind(&server::onAccept, this, new_participant, _1)));
        }
    
        void onAccept(std::shared_ptr<personInRoom> new_participant, const boost::system::error_code& error)
        {
            if (!error)
            {
                new_participant->start();
            }
    
            run();
        }
    
        boost::asio::io_service& io_service_;
        boost::asio::io_service::strand& strand_;
        tcp::acceptor acceptor_;
        chatRoom room_;
};




//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
    int iport=6666;
    
    
    try
    {


        std::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
        boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(*io_service));
        boost::shared_ptr<boost::asio::io_service::strand> strand(new boost::asio::io_service::strand(*io_service));



        std::list < std::shared_ptr < server >> servers;


        
        if (argc>1)
            iport=std::atoi(argv[1]);
        
       
        tcp::endpoint endpoint(tcp::v4(), iport);
        std::shared_ptr<server> a_server(new server(*io_service, *strand, endpoint));
        servers.push_back(a_server);
        
        std::cout << getTimestamp() << " 多人聊天机器人启动成功，服务端口为:" << iport <<std::endl;
        

        boost::thread_group workers;
        for (int i = 0; i < 1; ++i)
        {
            boost::thread * t = new boost::thread{ boost::bind(&workerThread::run, io_service) };

#ifdef __linux__
            // bind cpu affinity for worker thread in linux
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            pthread_setaffinity_np(t->native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
            workers.add_thread(t);
        }

        workers.join_all();
    } catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
