import yt_webvtt

clean_text, pos_and_time = yt_webvtt.read_webvtt("test")
print(clean_text, pos_and_time)
