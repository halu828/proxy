#!/usr/bin/env ruby
# encoding : utf-8

=begin
このRubyプログラムは，プロキシから呼び出されているので，ここでの標準出力は全てプロキシへ渡される．
ここでやるべきことは，
  HTTPヘッダの削除
  ボディ部の抜き出しとHTMLタグの取り除き
  ボティ部の文章を分かち書きして，頻出度を調べておく
  頻出度の高い単語をword2vecに突っ込んで，一番初めの単語を抜き出す．  その単語をオススメの単語として表示させる．
  もとのパケットを改変するのは，例のRecommendationのやつにして，そのRecommendationにおすすめ単語を表示．
  こうなってくるといよいよ，うまいことRecommendationを突っ込む方法を考えないといけない．
    現状は，mydistanceの表示をただbody部に埋め込むだけでいいか．単語はtokyoで．
=end

# 一時ファイルの中身を取得
tmphttp = File.read("tmphttp.txt", :encoding => Encoding::UTF_8)

# Rubyからword2vecを動かすmydistanceを呼び出す
value = `./mydistance vectors.bin shizuoka`
value = value.gsub(/\n/, "<br>")
# puts value

# bodyタグのすぐあとにmydistanceの出力を連結
tmphttp.scan(/<body.*>/) do |matched|
	puts tmphttp.sub(/<body.*>/, matched + "\n<p>" + value + "</p>\n")
end
