#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>

#define SIZE 50000
#define HOSTSIZE 128

void process0(int browserSocket) {
	struct sockaddr_in serverHost;
	struct hostent *ServerInfo;
	int serverSocket = 0;
	int maxSock;
	int recvSize; /* 受信したサイズを入れておく変数 */
	char buf[SIZE]; /* 文字列を入れておく配列 */
	char hostName[HOSTSIZE];
	char *get_post;
	char *pointer;
	fd_set def; /* 初期値を保持 */
	fd_set rfds; /* select()に必要 */

	/* bufの中身を初期化 */
	memset(buf, '\0', sizeof(buf));

	/* ブラウザからのリクエストを受信 */
	if ((recvSize = recv(browserSocket, buf, sizeof(buf), 0)) == -1) {
		fprintf(stderr, "ブラウザからの受信に失敗．プロセス終了．\n");
		close(browserSocket);
		close(serverSocket);
		exit(EXIT_SUCCESS);
	}

	/* ホスト名の抽出とリクエストの編集 */
	get_post = strtok(buf, " ");
	strtok(NULL, "/");
	pointer = strtok(NULL, "/");
	strcpy(hostName, pointer);
#if defined(DEBUG)
	fprintf(stdout, "ホスト名:%s\n", hostName);
#endif
	pointer += strlen(pointer) + 1;
	sprintf(buf, "%s /%s",get_post, pointer);

#if defined(DEBUG)
	fprintf(stdout, "--------[ブラウザからのリクエスト]--------\n%s",buf);
	/*fprintf(stdout, "------------------------------------------\n");*/
#endif


	/* hostNameからwebサーバのIPアドレスを求める */
	if ((ServerInfo = gethostbyname(hostName)) == NULL) {
		fprintf(stderr, "Failed to find host!\n");
		exit(1);
	}

	/* サーバとの通信用のソケットを作成 */
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Failed to make a serverSocket!\n");
		exit(1);
	}

	serverHost.sin_family = AF_INET;
	serverHost.sin_port = htons(80); /* webサーバならば通常は80を代入(well-known port) */
	memcpy((char *)&serverHost.sin_addr, (char *)ServerInfo->h_addr_list[0], ServerInfo->h_length);

	/* webサーバと接続 */
	if (connect(serverSocket, (struct sockaddr *)&serverHost, sizeof(serverHost)) == -1) {
		fprintf(stderr, "Failed to connect with the web server!\n");
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	/* サーバへリクエストを転送 */
	if (send(serverSocket, buf, recvSize, 0) == -1) {
		fprintf(stderr, "Failed to send!\n");
		exit(1);
	}

	/* 監視対象をセット */
	FD_ZERO(&def);
	FD_SET(browserSocket, &def);
	FD_SET(serverSocket, &def);

	if (browserSocket > serverSocket) {
		maxSock = browserSocket;
	} else {
		maxSock = serverSocket;
	}


	while(1) {
		/* bufの中身を初期化 */
		memset(buf, '\0', sizeof(buf));

		/* 初期値をコピーする */
		memcpy(&rfds, &def, sizeof(fd_set));

		/* browserSocketまたはserverSocketが読み取り可能になるまで待機 */
		if (select(maxSock+1, &rfds, NULL, NULL, NULL) <= 0) {
			continue; /* select()に失敗するとwhile文をやり直す */
		}

		/* ブラウザから受信できるなら */
		if (FD_ISSET(browserSocket, &rfds)) {
			fprintf(stdout, "ブラウザから受信．\n");
			if ((recvSize = recv(browserSocket, buf, sizeof(buf), 0)) <= 0) {
				fprintf(stderr, "ブラウザからのrecv()の返り値が0以下．プロセス終了．\n");
				break; /* 受信に失敗したらbreak */
			}

			/* ホスト名の抽出とリクエストの編集 */
			if (strncmp(buf, "GET", 3) == 0 || strncmp(buf, "POST", 4) == 0) {
				get_post = strtok(buf, " ");
				strtok(NULL, "/");
				pointer = strtok(NULL, "/");
				strcpy(hostName, pointer);
			#if defined(DEBUG)
				fprintf(stdout, "ホスト名:%s\n", hostName);
			#endif
				pointer += strlen(pointer) + 1;
				sprintf(buf, "%s /%s",get_post, pointer);
			}
			if (send(serverSocket, buf, recvSize, 0) == -1) {
				fprintf(stderr, "Failed to send!\n");
				exit(1);
			}

		#if defined(DEBUG)
			fprintf(stdout, "--------[ブラウザからのリクエスト]--------\n%s\n", buf);
			/*fprintf(stdout, "--------------------------------------\n");*/
		#endif
		}

		/* サーバから受信できるなら */
		if (FD_ISSET(serverSocket, &rfds)) {
			fprintf(stdout, "サーバから受信．\n");
			if ((recvSize = recv(serverSocket, buf, sizeof(buf), 0)) <= 0) {
				fprintf(stderr, "サーバからのrecv()の返り値が0以下．プロセス終了．\n");
				break; /* 受信に失敗したらbreak */
			}
		#if defined(DEBUG)
			fprintf(stdout, "---------[サーバからのレスポンス]---------\n%s\n", buf);
			/*fprintf(stdout, "---------------------------------------\n");*/
		#endif
			if (send(browserSocket, buf, recvSize, 0) == -1) {
				fprintf(stderr, "Failed to send!\n");
				exit(1);
			}
			fprintf(stdout, "[ブラウザへレスポンスを転送]\n");
		}

	}

	close(serverSocket);
	printf("サーバソケットを閉じました．\n");
	exit(EXIT_SUCCESS);
}
