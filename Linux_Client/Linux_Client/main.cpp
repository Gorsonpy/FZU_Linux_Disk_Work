#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#include <pthread.h>
void net_disk_ui() {
    printf("=========================TCP网盘客户端====================================\n");
    printf("========================功能菜单====================================\n");
    printf("==================1.查询文件========================================\n");
    printf("==================2.下载文件========================================\n");
    printf("==================3.上传文件========================================\n");
    printf("==================4.刷新菜单========================================\n");
    printf("==================0.退出系统========================================\n");
    printf("====================================================================\n");
}

#define MSG_TYPE_FILE_NAME 1
#define MSG_TYPE_LOGIN 0
#define MSG_TYPE_DOWNLOAD 2
#define MSG_TYPE_UPLOAD 3
#define MSG_TYPE_UPLOAD_DATA 4
#define MSG_TYPE_UPLOAD_SHOW 5

typedef struct msg {
    int type; // 操作类型
    int flag;
    char buffer[1024];
    char fname[50];
    char show_fname[100];
    int bytes;
}MSG;

char rootDir[100] = { "/home/gorsonpy/" };
char rootUpload[100] = { "/home/gorsonpy/projects/Linux_Client/bin/x64/Debug/download" };
char up_file_name[100] = { 0 };
MSG recv_msg = { 0 };
int fd = -1; //用来打开文件进行读写的文件描述,默认情况下为0表示还没打开

void* upload_file_thread(void* args) {
    MSG up_file_msg = { 0 };
    int client_socket = *((int*)args);
    int fd = -1;
    int res = 0;
    fd = open("./download/b.txt", O_RDONLY);
    if (fd < 0) {
        perror("open up file error:");
        return NULL;
    }
    up_file_msg.type = MSG_TYPE_UPLOAD_DATA;
    while ((res = read(fd, up_file_msg.buffer, sizeof(up_file_msg.buffer))) > 0) {
        printf("res : %d\n", res);
        up_file_msg.bytes = res;
        res = write(client_socket, &up_file_msg, sizeof(MSG));
        if (res <= 0) {
            perror("client send file error:");
        }
        memset(up_file_msg.buffer, 0, sizeof up_file_msg.buffer);
    }
}
void* thread_func(void* arg) {
    int client_socket = *((int*)arg);
    int res;
    char buffer[50] = { 0 };
    while (1) {
        res = read(client_socket, &recv_msg, sizeof(MSG));
        if (recv_msg.type == MSG_TYPE_FILE_NAME) {
            printf("server path filename = %s\n", recv_msg.fname);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD) {
            if (mkdir("download", S_IRWXU) < 0) {
                if (errno == EEXIST) {
                    
                }else {
                    perror("mkdir error\n");
                }
            }
            // 目录创建没问题之后就要开始创建文件
            if (fd == -1) {
                // 第一次打开
                fd = open("./download/a.txt", O_CREAT | O_WRONLY, 0666);
                if (fd < 0) {
                    perror("file open error:");
                }
            }
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (res < 0) {
                perror("file write error\n:");
            }
            if (recv_msg.bytes < sizeof(recv_msg.buffer)) {
                printf("file download finish!\n");
                close(fd);
                fd = -1;
            }
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD_SHOW) {
            if(!recv_msg.flag) printf("%s\n", recv_msg.fname);
            memset(recv_msg.fname, 0, sizeof(recv_msg.fname));
        }
    }
}
int main()
{
    int client_socket;
    pthread_t pthread_id, pthread_send_id;
    struct sockaddr_in server_addr; // 填写连接到服务器的ip地址和端口号
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {
        perror("clinet sokcet faile:");
        return 0;
    }
    char buffer[50] = { 0 };
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //如果服务器和客户端在同一台机子上，ip地址可以设为127.0.0.1
    server_addr.sin_port = htons(6666);

    // 创建好套接字后，连接到服务器
    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect error!");
        return 0;
    }
    int res;
    printf("客户端连接服务器成功!\n");

    pthread_create(&pthread_id, NULL, thread_func, &client_socket);

    char c; // 功能类型
    MSG send_msg = { 0 };
    net_disk_ui();
    while (1) {   
        c = getchar();
        switch (c) {
            case '1': 
                send_msg.type = MSG_TYPE_FILE_NAME;
                memcpy(send_msg.show_fname, rootDir, sizeof rootDir);
                printf("dir : %s\n", send_msg.show_fname);
                res = write(client_socket, &send_msg, sizeof(MSG));
                if (res < 0) {
                    perror("发送服务器失败\n");
                }
                memset(&send_msg, 0, sizeof(MSG));
                break;
            case '2':
                send_msg.type = MSG_TYPE_DOWNLOAD;
                res = write(client_socket, &send_msg, sizeof(MSG));
                if (res < 0) {
                    perror("send msg error\n");
                }
                memset(&send_msg, 0, sizeof(MSG));
                break;
            case '3':
                printf("download目录下文件有：\n");
                send_msg.type = MSG_TYPE_UPLOAD_SHOW;
                strcpy(send_msg.show_fname, rootUpload);
                res = write(client_socket, &send_msg, sizeof(MSG));
                memset(send_msg.show_fname, 0, sizeof send_msg.show_fname);

                while (!recv_msg.flag) {
                    // 还没收集完文件名                
                    sleep(0.1);
                }

                send_msg.type = MSG_TYPE_UPLOAD;
                printf("input up load filename:");
                scanf("%s", up_file_name);
                strcpy(send_msg.fname, up_file_name);
                res = write(client_socket, &send_msg, sizeof(MSG));
                if (res < 0) {
                    perror("send up load packg error:");
                    continue;
                }
                memset(&send_msg, 0, sizeof(MSG));
                // 上传文件需要比较长的时间 需要一个新的线程来专门处理文件上传任务
                pthread_create(&pthread_send_id, NULL, upload_file_thread, 
                    &client_socket);
                break;
            case '4':
                net_disk_ui();
                break;
            case '0':
                break;
        }
        memset(&send_msg, 0, sizeof send_msg);
    }
   
    return 0;
}