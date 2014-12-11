#!/usr/bin/env ruby
# encoding : utf-8
=begin
File.open('tmp.txt', "r:utf-8" ) do |f|
	while line  = f.gets
		puts(line)
	end
end
=end

=begin
f = open("recommendation.txt")
while line = f.gets
	puts(line)
end
f.close
f = open("tmphttp.txt")
while line = f.gets
	puts(line)
end
f.close
=end

# recommendation = File.read("recommendation.txt", :encoding => Encoding::UTF_8)
# recommendation = "<a href=\"http://localhost/floating2.html\">Recommendation</a>"
=begin
head = "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.0/jquery.min.js\"></script>\n"
body = File.read("body.txt", :encoding => Encoding::UTF_8)
tmphttp = File.read("tmphttp.txt", :encoding => Encoding::UTF_8)
tmphttp.scan(/<head*>/) do |matched|
	result = tmphttp.sub(/<head*>/, matched + "\n" + head)
	result.scan(/<body*>/) do |matched|
		puts result.sub(/<body*>/, matched + "\n" + body)
	end
end
=end

recommendation = "<a href=\"http://localhost/floating2.html\">Recommendation</a>"
tmphttp = File.read("tmphttp.txt", :encoding => Encoding::UTF_8)
# 一時ファイルから読み込むのではなく，C言語からpopenでもらう．→現状は一時ファイルの方向で．
# tmphttp = gets.chmop
tmphttp.scan(/<body*>/) do |matched|
	puts tmphttp.sub(/<body*>/, matched + "\n" + recommendation + "\n")
end
