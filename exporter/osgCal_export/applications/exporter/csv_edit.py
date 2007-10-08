#! /usr/bin/env python

# Copyright (C) 2007 Vladimir Shabanov <vshabanoff@gmail.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


usage = \
"""\
Usage: csv_edit.py [options] files
Options:
  -a <file>, --add <file>
        Add lines from other csv file. Only lines with new keys added.
        Key is a value in first column.
  -c, --create-if-not-exists
        Create editing files when they not exists
  -h, --help
        Display this list of options
  -r <file>, --remove <file>
        Remove lines with keys from specified file
  --sync-no-overwrite <file>
        Add lines with new keys and remove lines with keys that are
        no more exists\
"""

import getopt, sys, os, copy

def main():
    try:
        optlist, files = getopt.gnu_getopt(sys.argv[1:],
                                           "a:chr:",
                                           ["add=",
                                            "create-if-not-exists",
                                            "help",
                                            "remove=",
                                            "sync-no-overwrite="])
    except getopt.GetoptError:
        # print help information and exit:
        print usage
        sys.exit(2)

    create_if_not_exists = False
    actions = []
    for o, a in optlist:
        if o in ("-h", "--help"):
            print usage
            sys.exit()
        if o in ("-a", "--add"):
            actions += [add_keys(read_key_line_list(a))]
        if o in ("-c", "--create-if-not-exists"):
            create_if_not_exists = True
        if o in ("-r", "--remove"):
            actions += [remove_keys(read_key_line_list(a))]
        if o in ("--sync-no-overwrite"):
            actions += [sync_keys(read_key_line_list(a))]

    if len(files) == 0:
        print "csv_edit.py: no input files"
        sys.exit(2)

    for f in files:
        if not os.path.exists(f):
            if create_if_not_exists:
                file(f, "w").close()
            else:
                print "csv_edit.py: file", f, "does not exists"
                sys.exit(3)
        kl_orig = read_key_line_list(f)
        kl = copy.copy(kl_orig) # otherwise '!= operator' always shows
                                # that kl and kl_orig are equal
        for a in actions:
            kl = a(kl)
        if kl != kl_orig:
            write_key_line_list(kl, f)

def key(s):
    "Return value of first column in line from csv-file"
    return s.split(";")[0]

def read_key_line_list(file_name):
    "Read file into array of (key, line) tuples"
    f = file(file_name, "r")
    return map(lambda line: (key(line), line), f.readlines())

def write_key_line_list(kll, file_name):
    "Write (key, line) tuples array to the file"
    f = file(file_name, "w")
    f.writelines([line for k, line in kll])
    f.close()

def add_keys(akll):
    "Funtion which return action for adding keys"
    def add_keys_action(kll):
        kll_dict = dict(kll)
        for akey, aline in akll:
            if not kll_dict.has_key(akey):
                kll += [(akey, aline)]
                kll_dict[akey] = aline
        return kll
    return add_keys_action

def remove_keys(rkll):
    "Funtion which return action for removing keys"
    def remove_keys_action(kll):
        for rkey, rline in rkll:
            kll = filter(lambda (k,l): k!=rkey,
                         kll)
        return kll
    return remove_keys_action

def sync_keys(skll):
    "Funtion which return action for syncing keys."
    "Syncing adds new keys and remove keys that no more exists."
    def sync_keys_action(kll):
        rmkll = remove_keys(skll)(kll)
        kll = remove_keys(rmkll)(kll)
        kll = add_keys(skll)(kll)
        return kll
    return sync_keys_action

if __name__ == "__main__":
    main()
