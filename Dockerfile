# proxy
# CentOSのがいいのかなあ？

# OS
FROM ubuntu
#FROM centos:centos6

# 作成者
MAINTAINER Takumi Wakasa <cs11100@s.inf.shizuoka.ac.jp>

# アップデート
RUN apt-get update -y
RUN apt-get upgrade -y

# makeコマンド実行に必要
RUN apt-get install -y make gcc g++

# ifconfigのインストール
RUN apt-get install -y net-tools

# カレントディレクトリをコピー
ADD . /src

# ポート10080番を開放
EXPOSE 10080

# 作業ディレクトリを指定
WORKDIR src

# コンパイル(コンテナ上の環境の上でコンパイルしないと実行できない)
RUN make

# ipaddr.shに実行権限を付加する
RUN chmod +x ipaddr.sh

# シェルスクリプト実行
CMD ./ipaddr.sh
