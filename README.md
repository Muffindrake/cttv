# cttv
a terminal-based program for managing following streamers on different platforms

![preview](https://i.imgur.com/3tBvWp1.png)
- bottom text: service[online count]{ytdl quality x of n chosen}(quality string)timestamp days:hoursminuteseconds

This is a program used for better managing of different streaming
services to reduce the fatigue, memory usage and general inefficiency of having
to browse multiple tabs in a web browser.
It uses either youtube-dl or streamlink to play a stream in mpv.

Refer to the manpage `cttv.1` for general usage instructions.

# installation

```
$ git clone https://github.com/muffindrake/cttv
$ cd cttv
$ make
$ su -c "make install"
```

You may also want to execute the repo-provided shell script `placeconfigs.sh`
as a regular user to have necessary configuration files, without which the
program will not start, placed into the appropriate directories.

# dependencies
- C11-compliant compiler
- mpv
- youtube-dl
- streamlink
- sh-compatible shell

The following libraries need to be present, runtime and development headers:
- libcurl
- jansson
- ncurses
- glib-2.0

# license
see the LICENSE file
