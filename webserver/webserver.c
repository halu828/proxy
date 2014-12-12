/* このままだと，同時接続には耐えられないが，コンテナ1つにつき1ユーザと考えるとこれでも良い．
 * もしコンテナ1つにつき1ユーザでなくなった場合は，マルチプロセスに変更する．
 * コンパイル: $ gcc -o webserver webserver.c
 * usage: $ sudo ./webserver 80
 	$ sudo ./webserver
 		この場合，勝手に80番ポートが設定される．
 	ブラウザでhttp://localhost/index.htmlにアクセス
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

/*#define DOCUMENT_ROOT "./htdocs"*/
#define BUF_SIZE 1024

#define DEBUG

int sockfd;

/* Ctrl+Cの割り込みで行う処理 */
void SigHandler(int SignalName) {
	fprintf(stderr, "\nWebサーバを終了します．\n");
	close(sockfd);
	exit(0);
}


int main(int argc, char *argv[]) {
	struct sockaddr_in server;
	int port = (argc == 2) ? atoi(argv[1]) : 80;
	char path[512] = "";
	time_t timer; /* 年月日と時刻 */
	struct tm *local; /* 年月日と時刻 */

	/* SIGINTにSigHandler()を登録 */
	signal(SIGINT, SigHandler);

	/* カレントディレクトリ取得 */
	getcwd(path, 512);

	/* socket作成 */
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	/* Port, IPアドレスを設定(IPv4) */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	/* bind()失敗対策 */
	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

	/* ディスクリプタとポート番号の設定 */
	if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	/* listen準備 */
	if (listen(sockfd, SOMAXCONN) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* clientからの接続待ちとレスポンスを返すループ */
	while (1) {
		struct sockaddr_in client;
		int newfd;

		char buf[BUF_SIZE] = "";
		char method[BUF_SIZE] = "";
		char url[BUF_SIZE] = "";
		char protocol[BUF_SIZE] = "";
		int len;

		int filefd;
		char file[BUF_SIZE] = "";

		/* clientからの接続を受け付ける */
		memset(&client, 0, sizeof(client));
		len = sizeof(client);
		if ((newfd = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&len)) < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		/* request line読み込み */
		if (recv(newfd, buf, sizeof(buf), 0) < 0) {
			perror("recv");
			exit(EXIT_FAILURE);
		}
		/* 現在時刻を取得 */
		timer = time(NULL);
		local = localtime(&timer); /* 地方時に変換 */

		/* bufからメソッドとURLとプロトコルを取得 */
		sscanf(buf, "%s %s %s", method, url, protocol);

		/* 年月日と現在時刻を表示 */
		printf("%4d/%02d/%02d %02d:%02d:%02d %s\n", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec, url);

		/* request headerの終わりまで読み飛ばし */
		/* bodyは無視	 */
		do {
			if (strstr(buf, "\r\n\r\n")) {
				break;
			}
			if (strlen(buf) >= sizeof(buf)) {
				memset(&buf, 0, sizeof(buf));
			}
		} while (recv(newfd, buf+strlen(buf), sizeof(buf) - strlen(buf), 0) > 0);

		/* ファイルパス生成（脆弱性有り）	 */
		sprintf(file, path, 512);
		strcat(file, url);

		/* index.html補完 */
		if (file[strlen(file)-1] == '/') {
			strcat(file, "index.html" );
		}

		/* debug */
		#ifdef DEBUG
			sleep(1);
		#endif

		/* body送信
		 * ファイルが開けなかった場合は404のヘッダと404.htmlを送る
		 * 無事ファイルが開けた場合は200のヘッダと要求されたファイルを送る
		 */
		char *header;
		if ((filefd = open(file, O_RDONLY)) < 0 || strstr(file, "..") != NULL) {
			perror("open");
			fprintf(stderr, "file: %s\n", file);
			sprintf(file, "%s/404.html", path); /* [カレントディレクトリ]/404.html */
			filefd = open(file, O_RDONLY);
			header = "HTTP/1.1 404 Not Found\n"
						"Content-type: text/html\n"
						"\n";
		} else {
			header = "HTTP/1.0 200 OK\n"
						"Content-type: text/html\n"
						"\n";
		}

		send(newfd, header, strlen(header), 0);
		while ((len = read(filefd, buf, sizeof(buf))) > 0) {
			if (send(newfd, buf, len, 0) < 0) {
				perror("send2");
			}
		}

		close(newfd);
	}
}
