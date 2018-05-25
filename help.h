#ifndef HELP_H
#define HELP_H

#define HELPTEXT \
"cttv: a program for managing following streamers on different platforms\n\n"\
"to use, place files containing channel names in either $XDG_CONFIG_HOME/cttv "\
"or $HOME/.config/cttv, depending on whether XDG_CONFIG_HOME is set, in plain "\
"text files named according to the information in the listing of available "\
"stream services.\n"

void help_print(void);

#endif
