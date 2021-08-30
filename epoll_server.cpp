#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <unordered_map>
#include <sys/stat.h>


using namespace std;

#define MAXLINE 6
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5000
#define INFTIM 1000

int FileSize(const char* fname)
{
    struct stat statbuf;
    if(stat(fname , &statbuf) == 0)
        return statbuf.st_size;
    return -1;
}

void setnonblocking(int sock)
{
    int opts;
    opts=fcntl(sock,F_GETFL);
    if(opts<0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

string getfilename(string receive)
{
    int indexfirst = receive.find("/",0)+1;
    int indexsecond = receive.find(" ",indexfirst);
    string ret = receive.substr(indexfirst,indexsecond-indexfirst);
    return ret;
}
int main(int argc, char* argv[])
{
    unordered_map<int ,int > fd_file;
    int i, maxi, listenfd, connfd, sockfd,epfd,nfds, portnumber;
    ssize_t n;
    char line[MAXLINE];
    socklen_t clilen;

    if ( 2 == argc )//命令行参数
    {
        if( (portnumber = atoi(argv[1])) < 0 )
        {
            fprintf(stderr,"Usage:%s portnumber/a/n",argv[0]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr,"Usage:%s portnumber/a/n",argv[0]);
        return 1;
    }

    struct epoll_event ev,events[20];
    //生成用于处理accept的epoll专用的文件描述符

    epfd=epoll_create(256);
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    ev.data.fd=listenfd;
    //设置要处理的事件类型
    ev.events=EPOLLIN;
    //ev.events=EPOLLIN;
    //注册epoll事件
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);//把监听用的socket
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    char *local_addr="127.0.0.1";
    inet_aton(local_addr,&(serveraddr.sin_addr));//htons(portnumber);
    serveraddr.sin_port=htons(portnumber);
    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));
    listen(listenfd, LISTENQ);
    maxi = 0;
    while(1) {
        //等待epoll事件的发生
        nfds=epoll_wait(epfd,events,20,500);//events事件合集，20不能大于epoll_create()时的size,500毫秒超时时间，0会立即返回
        //处理所发生的所有事件
        for(i=0;i<nfds;++i)
        {
            if(events[i].data.fd==listenfd)//如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
            {
                connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
                if(connfd<0){
                    perror("connfd<0");
                    exit(1);
                }
                //setnonblocking(connfd);
                char *str = inet_ntoa(clientaddr.sin_addr);
                ev.data.fd=connfd; 
                ev.events=EPOLLIN;
                //注册ev
                epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev);
            }
            else if(events[i].events&EPOLLIN)//如果是已经连接的用户，并且收到数据，那么进行读入。
            {
                if ( (sockfd = events[i].data.fd) < 0)continue;
                
                ev.data.fd=sockfd;
                ev.events=EPOLLOUT;
                epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);
                
                char s[1024];
                read (sockfd, s, sizeof(s));
                string receive = s;
                cout << receive << endl;
                string filename = getfilename(receive);
                string filepath = "./"+filename;
                int file = open(filename.c_str(),O_RDONLY);
                fd_file[sockfd]=file;//存入

                string header = "HTTP/1.1 200 OK\r\n";

                int content_length = FileSize(filepath.c_str());
                string content_length_string ="Content-Length:"+to_string(content_length)+"\r\n\r\n";

                string respond = header+content_length_string;
                char buffer [1024];
                read(file,buffer,sizeof(buffer));
                write(sockfd,respond.c_str(),respond.size());
                write(sockfd,buffer,sizeof(buffer));
                cout <<write<<endl;
                cout <<respond<<endl;
            }
            else if(events[i].events&EPOLLOUT) // 如果有数据发送
            {
                sockfd = events[i].data.fd;
                // ev.data.fd=sockfd;
                // ev.events=EPOLLOUT;
                // epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&e)v;
                char buffer[1024];
                int file = fd_file[sockfd];
                int stop = read(file,buffer,sizeof(buffer));
                if(stop == 0) continue;
                else write(sockfd,buffer,sizeof(buffer));
            }
        }
    }
    return 0;
}