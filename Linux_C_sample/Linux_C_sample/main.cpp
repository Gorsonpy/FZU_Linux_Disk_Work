#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef struct msg {
    int type; // 操作类型
    int flag;
    char buffer[1024];
    char fname[50];
    char show_fname[100];
    int bytes; // 用来记录传输文件的时候，每个数据包实际的文件字节数
}MSG;

#define MSG_TYPE_LOGIN 0
#define MSG_TYPE_FILE_NAME 1
#define MSG_TYPE_DOWNLOAD 2
#define MSG_TYPE_UPLOAD 3
#define MSG_TYPE_UPLOAD_DATA 4
#define MSG_TYPE_DOWNLOAD_SHOW 5

MSG recv_msg = { 0 };
char rootdir[30] = { "/home/gorsonpy/" };
char download_from_client_dir[200] = {"/home/gorsonpy/projects/Linux_C_sample/bin/x64/Debug/download_from_client/"};
char download_dir[100] = { "/home/gorsonpy/projects/Linux_C_sample/bin/x64/Debug/download/" };
// 服务端查看服务器这边目录下的文件名信息
// 默认情况下服务器的目录我们设置为用户的家目录/home
void search_server_dir(int accept_socket) {
    struct dirent* dir = NULL;
    MSG info_msg = { 0 };
    info_msg.type = recv_msg.type;
    int res = 0;

    DIR* dp = NULL;
    if (info_msg.type == MSG_TYPE_FILE_NAME) dp = opendir(rootdir);
    else dp = opendir(download_dir);
    if (NULL == dp) {
        perror("open dir error:\n");
        return;
    }
    while (1) {
        dir = readdir(dp);
        // 返回是空表示文件夹全部读取完成
        if (NULL == dir) {
            info_msg.flag = true;
            res = write(accept_socket, &info_msg, sizeof(MSG));
            if (res < 0) {
                perror("flag send error:");
                return;
            }
            info_msg.flag = false;
            break;
        }
        if (dir->d_name[0] != '.') {
            /*printf("name=%s\n", dir->d_name);*/
            memset(info_msg.fname, 0, sizeof info_msg.fname);

            // 发送给客户端
            strcpy(info_msg.fname, dir->d_name);
            res = write(accept_socket, &info_msg, sizeof(MSG));
            if (res < 0) {
                perror("send client error:\n");
                return;
            }
        }
    }
}
void server_file_download(int accept_socket) {
    MSG file_msg = { 0 };
    int res = 0;
    int fd; // 文件描述符
    char dir[150] = { 0 };
    strcpy(dir, download_dir);
    strcat(dir, recv_msg.fname);

    printf("dir : %s\n", dir);
    fd = open(dir, O_RDONLY);
    if (fd < 0) {
        perror("file open error:");
        return;
    }
    file_msg.type = MSG_TYPE_DOWNLOAD;
    strcpy(file_msg.fname, recv_msg.fname);
    while ((res = read(fd, file_msg.buffer, sizeof(file_msg.buffer))) > 0) {
        file_msg.bytes = res;
        res = write(accept_socket, &file_msg, sizeof(MSG));
        if (res <= 0) {
            perror("server send file error : \n");
        }
        memset(file_msg.buffer, 0, sizeof(file_msg.buffer));
    }
}
void* thread_fun(void* arg) {
    int acpt_socket = *((int*)arg);
    int res;
    char buffer[50] = { 0 };
    int fd = -1;

    while (1) {
        // read函数就是接受客户端发来的数据, 返回值表示实际读取的字节数
           // buffer 收到客户端数据后把数据存放的地址，sizof buffer就是希望读取的字节数
        res = read(acpt_socket, &recv_msg, sizeof(MSG));
        if (!res) {
            printf("客户端已经断开\n");
            break;
        }
        if (recv_msg.type == MSG_TYPE_FILE_NAME  || recv_msg.type == MSG_TYPE_DOWNLOAD_SHOW) {
            search_server_dir(acpt_socket);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD) {
            server_file_download(acpt_socket);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD) {
            // 根据用户选择的文件名创建或者打开
            char dir[150] = { 0 };
            strcpy(dir, download_from_client_dir);
            strcat(dir, recv_msg.fname);

            fd = open(dir, O_CREAT | O_WRONLY, 0666);
            if (fd < 0) {
                perror("create up file error:");
            }
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD_DATA) {
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (recv_msg.bytes < sizeof recv_msg.buffer) {
                // 最后一部分
                printf("client up file ok!\n");
                close(fd);
            }
            memset(&recv_msg.buffer, 0, sizeof(recv_msg.buffer));
            memset(buffer, 0, sizeof buffer);
        }
        memset(&recv_msg, 0, sizeof(MSG));
        //write(acpt_socket, buffer, res);
        //memset(buffer, 0, sizeof buffer);
    }
}


int main()
{
    char buffer[50] = { 0 };
    int server_socket; // 套接字描述符
    // 创建套接字描述符，ipv4，流式
    pthread_t threadId;
    printf("开始创建TCP服务器:\n");
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket create failed:");
        return 0;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // ip地址，自动绑定网卡地址
    server_addr.sin_port = htons(6666); //端口号 htons是转化为网络字节顺序

    int optvalue = 1;
    //解决端口占用要设置的
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(int));
    // 把设定好的ip地址和端口号绑定到server_socket描述符
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("server bind error:");
        return 0;
    }

    // 监听程序
    if (listen(server_socket, 10) < 0) {
        perror("server listen error:");
        return 0;
    }

    printf("TCP服务器准备完成，等待客户端连接:\n");
    // 等待客户端连
 
    while (1) {
        int accept_socket = accept(server_socket, NULL, NULL);
        // 用accept_socket处理后续的访问
        printf("有客户端连接到服务器!\n");

        // 创建一个新的线程，线程函数创建成功之后，系统就会执行thread fun代码。而这里的代码就是多线程代码
        pthread_create(&threadId, NULL, thread_fun, &accept_socket);
    }
    return 0;
}