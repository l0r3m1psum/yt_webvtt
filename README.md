For some mysterious reason the
[WebVTT](https://developer.mozilla.org/en-US/docs/Web/API/WebVTT_API) files
(i.e. the ones used for subtitles) contains a lot of repetitions, maybe it is
some sort of fall-back. This makes them hard to use for text analysis purposes.

The C code in this repository and the accompanying Python module can be used to
cleanup this files. Below there is an example, given this piece of WebVTT file:

```
00:00:02.879 --> 00:00:05.430 align:start position:0%
 
good<00:00:03.120><c> afternoon</c>

00:00:05.430 --> 00:00:05.440 align:start position:0%
good afternoon
```

The cleaned version is the following were the second element in the tuple is a
list where every "pair" of numbers in it must be interpreted as follows the first is an offset in the string, the second it the timestamp tag converted in
milliseconds, positioned at that offset.

```
"good afternoon", [4, 3120]
```

# Build and install

```
rm -rf yt_webvtt.egg-info build dist
pip install .
python setup.py build && python setup.py install
```
