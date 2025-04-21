#define _GNU_SOURCE 1
#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h>
#include<fcntl.h>
#include <unistd.h>
#include <string.h>
#include<string>
#include <netdb.h>

using namespace std;
#define MAX_USER 10
#define MAX_BUFFER 1024
#define MAX_FD   65535
struct client_info
{
    sockaddr_in addr;
    char *write_buffer; // 改为固定大小的缓冲区
    char  read_buffer[MAX_BUFFER];
};

int setNonBloking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,old_option | O_NONBLOCK);
    return old_option;
}

int main(int argc, char *argv[])
{
    
    int port = 54321;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    //端口回收
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //绑定本机ip地址    
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ret = bind(sockfd,(struct sockaddr*)&addr,sizeof(addr));

    ret = listen(sockfd,MAX_USER);
    if (ret < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    struct pollfd fd[MAX_USER+1];
    client_info *users = new client_info[MAX_FD];
    
    for (int i = 0; i < MAX_USER +1;i++)
    {
        fd[i].fd = -1;
        fd[i].events = 0;
        fd[i].revents = 0;
    }

    int user_count = 0; //连接客户端数量
    fd[0].fd = sockfd;
    fd[0].events = POLLERR | POLLIN;
    while(true)
    {
        ret = poll(fd,user_count + 1,-1);
        if (ret < 0)
        {
            if (errno == EINTR) { // Interrupted by signal, continue
                continue;
           }
           perror("poll");
           break;
        }
        for(int i = 0;i < user_count + 1; ++i)
        {
            if ((fd[i].fd == sockfd) && (fd[i].revents & POLLIN))
            {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int accept_fd = accept(sockfd,(struct sockaddr*)&client,&client_len);
                if(accept_fd < 0)
                {
                    cout << "Fail to accept" << endl;
                    continue;
                }
                if(user_count >= MAX_USER)
                {
                    const char * c = "Fail to connect,too much connection";
                    send(accept_fd, c, strlen(c), 0);
                    close(accept_fd);
                    continue;
                }
                cout << "a new user connect" << endl;
                user_count++;
                setNonBloking(accept_fd);
                users[accept_fd].addr = client;
                fd[user_count].fd = accept_fd;
                fd[user_count].events = POLLIN | POLLRDHUP | POLLERR;
            }
            else if(fd[i].revents & POLLRDHUP)
            {
                //users[fd[i].fd] = users[0];
                memset(&users[fd[i].fd],0,sizeof(users[fd[i].fd]));
                close(fd[i].fd);
                fd[i] = fd[user_count];
                user_count--;
                i--;
                cout << "a user left" << endl;
            }
            else if(fd[i].revents & POLLIN)
            {
                char info[100];
                char temp[MAX_BUFFER];
                memset(info,'\0',100);
                memset(users[fd[i].fd].read_buffer,'\0',MAX_BUFFER);
                memset(temp,'\0',MAX_BUFFER);
                getnameinfo((struct sockaddr*)&users[fd[i].fd].addr,sizeof(users[fd[i].fd].addr),info,99,NULL,0,0);
                strcat(info,":");
                strcat(users[fd[i].fd].read_buffer,info);
                
                
                int rec = recv(fd[i].fd,temp,MAX_BUFFER-1,0);
                strcat(users[fd[i].fd].read_buffer,temp);
                //const struct sockaddr_in clt = users[fd[i].fd].addr;
                
                if (rec < 0)
                {
                    cout << "Fail to recv" << endl;
                    users[fd[i].fd] = users[0];
                    close(fd[i].fd);
                    fd[i] = fd[user_count];
                    i--;
                    user_count--;
                }
                else if(rec == 0)
                {
                    close(fd[i].fd);
                    fd[i] = fd[user_count];
                    i--;
                    user_count--;
                    continue;
                }
                else if(rec > 0)
                {
                    for (int j = 1; j < MAX_USER;++j)
                    {
                        if(fd[j].fd == fd[i].fd)
                        {
                            continue;
                        }
                        //fd[j].events |= ~POLLIN;
                        fd[j].events |= POLLOUT;
                        users[j].write_buffer= users[fd[i].fd].read_buffer;
                    }
                }
            }
            else if(fd[i].revents & POLLOUT)
            {
                if(!users[fd[i].fd].write_buffer)
                {
                    continue;
                }
                int sd = send(fd[i].fd,users[fd[i].fd].write_buffer,strlen(users[fd[i].fd].write_buffer),0);
                users[fd[i].fd].write_buffer = NULL;
                fd[i].events &= ~POLLOUT;
                //fd[i].events |= POLLIN;
            }
        }
    }
    delete[] users;
    close(sockfd);
    return 0;
}