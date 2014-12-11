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

/* 置換する．buf の中の front を behind にする．成功=1 失敗=0
 * keep-aliveのヘッダ編集，パケット改変に使用
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
	int recvSize; /* 受信したサイズを入れておく変数 */
	char buf[SIZE] = ""; /* パケットを入れておく配列 */
	char hostName[HOSTSIZE];
	char *front;
	char *behind;
	fd_set def; /* 初期値を保持 */
	fd_set rfds; /* select()に必要 */

	/* ブラウザからのリクエストを受信 */
	if ((recvSize = recv(browserSocket, buf, sizeof(buf), 0)) == -1) {
		fprintf(stderr, "ブラウザからの受信に失敗．プロセス終了．\n");
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
			fprintf(stdout, "サーバから受信．\n");
			if ((recvSize = recv(serverSocket, buf, sizeof(buf), 0)) <= 0) {
				fprintf(stderr, "サーバからのrecv()の返り値が0以下．プロセス終了．\n");
				break; /* 受信に失敗したらbreak */
			}

			/* HTMLを改変してみる */
			/*if (strcasestr(buf, "<head>") != NULL){
				strrep(buf, "<head>", "<head>\n<script type=\"text/javascript\">\n<!--\nfunction disp() {\nif(confirm(\"オススメまとめページへ飛びますか？\")==true){\nlocation.href = \"http://localhost:8001/index.html\";\n}\n}\n-->\n</script>");
			}
			if (strcasestr(buf, "<body>") != NULL){
				strrep(buf, "<body>", "<body>\n<p><input type=\"button\" name=\"link\" value=\"あなたにおすすめのサイト\" onClick=\"disp()\"></p>");
			}*/

			/* localhostは，WebサーバのIPアドレスであるべき→そうした */
			/*if (strcmp(hostName, ipaddress) != 0) {
				if (strcasestr(buf, "<body>") != NULL) {
					char tmp[512];
					//sprintf(tmp, "<body>\n<a href=\"http://%s/index.html\">Recommendation</a>", ipaddress);
					sprintf(tmp, "<body>\n<iframe src=\"http://%s/index.html\" width=\"500\" height=\"300\"></iframe>", ipaddress);
					strrep(buf, "<body>", tmp);
				}
			}*/

			/*if (strcmp(hostName, ipaddress) != 0) {
				char *tp;
				if (tp = strcasestr(buf, "<body") != NULL) {
					strtok();
					FILE *fp;
					char *fname = "recommendation.txt";
					char s[512];
					char tmp[4096];

					strcat(tmp, "<body>\n");
					fp = fopen(fname, "r");
					if(fp == NULL){
						printf( "%sファイルが開けません\n", fname);
						exit(1);
					}
					while(fgets(s, 512, fp) != NULL){
						strcat(tmp, s);
					}
					strrep(buf, "</html>", tmp);
					fclose(fp);
				}
			}*/
			/* ここでテキストを別なプログラムへ投げて，処理させる？ */

			#if defined(DEBUG)
				printf("---------[レスポンス]---------\n%s\n", buf);
			#endif
			/* ブラウザへレスポンスを転送 */
			if (send(browserSocket, buf, recvSize, 0) == -1) {
				fprintf(stderr, "Failed to send!\n");
				exit(1);
			}
			printf("[ブラウザへレスポンスを転送]\n");
		}

	}

	close(serverSocket);
	printf("サーバソケットを閉じました．\n");
	exit(EXIT_SUCCESS);
}
