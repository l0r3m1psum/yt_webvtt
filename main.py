import yt_webvtt

text = open('FOMC press conference, November 3, 2021.en.vtt').read()
res = yt_webvtt.read_webvtt(text)
print(res)

error = yt_webvtt.read_webvtt("")
print(error)
