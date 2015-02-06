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
#include <err.h>

#define SIZE 32768

/* 置換する. buf の中の front を behind にする．成功=1 失敗=0
 * keep-aliveのヘッダ編集, パケット改変に使用
 */
int strrep(char *buf, char *front, char *behind) {
	char *ch;
	size_t frontlen, behindlen;

	frontlen = strlen(front);
	behindlen = strlen(behind);
	if (frontlen == 0 || (ch = strcasestr(buf, front)) == NULL) return 0;
	memmove(ch + behindlen, ch + frontlen, strlen(buf) - (ch + frontlen - buf ) + 1);
	memcpy(ch, behind, behindlen);
	return 1;
}

void process1(int browserSocket, char *ipaddress) {
	struct sockaddr_in serverHost;
	struct hostent *ServerInfo;
	int serverSocket = 0;
	int maxSock;
	int recvSize;					/* 受信したサイズを入れておく変数 */
	char buf[SIZE] = "";	/* リクエストやレスポンスを入れておく変数 */
	char hostName[128];		/* ホスト名 */
	char *front;					/* リクエスト編集に使用 */
	char *behind;					/* リクエスト編集に使用 */
	fd_set def;						/* 初期値を保持 */
	fd_set rfds;					/* select()に必要 */
	char *tp;							/* htmlコード保存に使用 */
	FILE *fp;							/* htmlコード保存に使用 */
	int flag = 0;					/* htmlコード保存に使用 */

	/* ブラウザからのリクエストを受信 */
	if ((recvSize = recv(browserSocket, buf, sizeof(buf), 0)) == -1) {
	#if defined(DEBUG)
		fprintf(stderr, "ブラウザからの受信に失敗．プロセス終了．\n");
	#endif
		close(browserSocket);
		close(serverSocket);
		exit(EXIT_SUCCESS);
	}

	/* ホスト名の抽出とリクエストの編集 */
	front = strtok(buf, " ");
	strtok(NULL, "/");
	behind = strtok(NULL, "/");
	strcpy(hostName, behind);
#if defined(DEBUG)
	printf("ホスト名:%s\n", hostName);
#endif
	behind += strlen(behind) + 1;
	sprintf(buf, "%s /%s",front, behind);
	strrep(buf, "Proxy-", ""); /* "Proxy-Connection"を"Connection"に変換 */

#if defined(DEBUG)
	printf("--------[ブラウザからのリクエスト]--------\n%s",buf);
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

	if (browserSocket > serverSocket) maxSock = browserSocket;
	else maxSock = serverSocket;


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
		#if defined(DEBUG)
			printf("ブラウザから受信．\n");
		#endif
			if ((recvSize = recv(browserSocket, buf, sizeof(buf), 0)) <= 0) {
			#if defined(DEBUG)
				fprintf(stderr, "ブラウザからのrecv()の返り値が0以下．プロセス終了．\n");
			#endif
				break; /* 受信に失敗したらbreak */
			}

			/* ホスト名の抽出とリクエストの編集 */
			if (strncmp(buf, "GET", 3) == 0 || strncmp(buf, "POST", 4) == 0) {
				front = strtok(buf, " ");
				strtok(NULL, "/");
				behind = strtok(NULL, "/");
				strcpy(hostName, behind);
			#if defined(DEBUG)
				printf("ホスト名:%s\n", hostName);
			#endif
				behind += strlen(behind) + 1;
				sprintf(buf, "%s /%s",front, behind);
				strrep(buf, "Proxy-", ""); /* "Proxy-Connection"を"Connection"に変換 */
			}

			/* サーバへリクエストを転送 */
			if (send(serverSocket, buf, recvSize, 0) == -1) {
				fprintf(stderr, "Failed to send!\n");
				exit(1);
			}

		#if defined(DEBUG)
			printf("--------[リクエスト]--------\n%s\n", buf);
		#endif
		}

		/* サーバから受信できるなら */
		if (FD_ISSET(serverSocket, &rfds)) {
		#if defined(DEBUG)
			printf("サーバから受信．\n");
		#endif
			if ((recvSize = recv(serverSocket, buf, sizeof(buf), 0)) <= 0) {
			#if defined(DEBUG)
				fprintf(stderr, "サーバからのrecv()の返り値が0以下．プロセス終了．\n");
			#endif
				break; /* 受信に失敗したらbreak */
			}

			#if defined(DEBUG)
				printf("---------[レスポンス]---------\n%s\n", buf);
			#endif
			/* ブラウザへレスポンスを転送 */
			if (send(browserSocket, buf, recvSize, 0) == -1) {
				fprintf(stderr, "Failed to send!\n");
				exit(1);
			}
		#if defined(DEBUG)
			printf("[ブラウザへレスポンスを転送]\n");
		#endif

			/* ここでbufをRubyへ投げて処理させる */
			// if (strcmp(hostName, ipaddress) != 0) {
				
				// if (strcasestr(buf, "Content-Type: text/html") != NULL) {
				// 	FILE *fp;
				// 	if ((fp = fopen("html.txt", "a")) == NULL) {
				// 		err(EXIT_FAILURE, "%s", "html.txt");
				// 	}
				// 	if (strcasestr(buf, "<body") != NULL) {
				// 	while (1) {
				// 		recvSize = recv(serverSocket, buf, sizeof(buf), 0);
				// 		send(browserSocket, buf, recvSize, 0);
				// 		// if (recvSize <= 0) break;
				// 		// if ((tp = strstr(buf, "\r\n\r\n")) != NULL) fprintf(fp, "%s", tp+1);
				// 		// else fprintf(fp, "%s", buf);
				// 		fprintf(fp, "%s", buf);
				// 		if (strcasestr(buf, "</body") != NULL) break;
				// 		memset(buf, '\0', sizeof(buf));
				// 	}
				// 	}
				// 	fclose(fp);
				// }

				/* htmlコードだけをhtml.txtに保存 */
				if (strcasestr(buf, "Content-Type: text/html") != NULL &&
					strcasestr(buf, "<body") != NULL) flag = 1;
				if (flag == 1) {
					if ((fp = fopen("html.txt", "a")) == NULL) {
						err(EXIT_FAILURE, "%s", "html.txt");
					}
					if ((tp = strstr(buf, "\r\n\r\n")) != NULL) fprintf(fp, "%s", tp+1);
					else fprintf(fp, "%s", buf);
					fclose(fp);
				}
				if (strcasestr(buf, "</body") != NULL) flag = 0;

		}

	}

	close(serverSocket);
#if defined(DEBUG)
	printf("サーバソケットを閉じました．\n");
#endif
	exit(EXIT_SUCCESS);
}
