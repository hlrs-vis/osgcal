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
Usage: export_textures.py [options]
Options:
  -o <dir>, --output-dir <dir>
        Path where textures will be exported
  -l <file.csv>, --textures-list <file.csv>
        File with list of textures to export.
        File must be csv with following format:
          <file name>;<compression>;<max width>;<max height>
        Where compression is one of:
          DXT1, DXT3, DXT5, RGB8, RGBA8, R5G6B5,
          DXT5_NM [-flipx true] [-flipy true],
          NormalsMap [-flipx true] [-flipy true],
          Bump
        For NormalsMap osgCalNMExport is used, for all
        other formats nvdxt is used.
  -p <file>, --paths-list <file>
        Textures search paths list
  --delete-other-textures
        All textures in output directory which are not
        listed in textures list will be deleted after export
  --changed-only
        Only export textures which was modified, i.e. have
        another MD5 checksum or different parameters
        in textures list.
  -h, --help
        Display this list of options\
"""

import getopt, sys, os, string, md5, binascii, traceback

def main():
    try:
        export_textures()
    except Exception:
        print "Exception occured while exporting textures:"
        traceback.print_exc(file=sys.stdout)
        sys.exit(2)

def export_textures():

    # Parse agrs
    
    try:
        optlist, args = getopt.gnu_getopt(sys.argv[1:],
                                          "o:l:p:h",
                                           ["output-dir=",
                                            "textures-list=",
                                            "paths-list=",
                                            "help",
                                            "delete-other-textures",
                                            "changed-only"])
    except getopt.GetoptError:
        # print help information and exit:
        print usage
        sys.exit(2)

    output_dir = None
    textures_list = None
    paths_list = []
    delete_other_textures = False
    changed_only = False
    
    for o, a in optlist:
        if o in ("-h", "--help"):            
            print usage
            sys.exit()
        if o in ("-o", "--output-dir"):
            output_dir = a
        if o in ("-l", "--textures-list"):
            textures_list = read_textures_list(a)
        if o in ("-p", "--paths-list"):
            paths_list = read_paths_list(a)
        if o in ("--delete-other-textures"):
            delete_other_textures = True
        if o in ("--changed-only"):
            changed_only = True

    if textures_list == None:
        print "export_textures.py: textures list not specified"
        sys.exit(2)

    if output_dir == None:
        print "export_textures.py: output directory not specified"
        sys.exit(2)   

    # Process textures

    current_textures_fname = os.path.join(output_dir, "textures.csv")
    
    try:
        current_textures = read_textures_list(current_textures_fname)
    except IOError:
        current_textures = []

    for texture, _, compression, max_width, max_height in textures_list:
        # Find texture in paths
        if os.path.exists(texture_name(texture)):
            source_texture_file_name = texture
        else:
            source_texture_file_name = find_file_in_paths_list(texture_name(texture), paths_list)
            if source_texture_file_name == None:
                print "Texture", texture_name(texture), "not found"
                sys.exit(2)

        source_texture_hash = file_hash(source_texture_file_name)

        # Update current textures list and check for parameter changes
        new_current_textures = []
        texture_parameters_changed = False
        texture_found = False
        for ctexture, chash, ccompression, cmax_width, cmax_height in current_textures:
            if ctexture == texture:
                texture_found = True
                texture_parameters_changed = (compression != ccompression or
                                              max_width != cmax_width or
                                              max_height != cmax_height or
                                              chash != source_texture_hash)
                new_current_textures += [(texture, source_texture_hash,
                                          compression, max_width, max_height)]
            else:
                new_current_textures += [(ctexture, chash, ccompression, cmax_width, cmax_height)]
        if texture_found:
            current_textures = new_current_textures
        else:
            current_textures += [(texture, source_texture_hash,
                                  compression, max_width, max_height)]

        target_texture = os.path.join(output_dir,
                                      dds_texture_name(texture))
        texture_display_name = source_texture_file_name
        type_of_texture = texture_type(texture)
        if type_of_texture != "":
            texture_display_name += " [" + type_of_texture + "]"

        texture = source_texture_file_name

        # Check texture modification state
        if changed_only:
            try:
                os.stat(target_texture) # try to get stat (to regenerate texture on error)
                #target_texture_mtime = os.stat(target_texture).st_mtime
                #texture_mtime = os.stat(texture).st_mtime
                if (#texture_mtime < target_texture_mtime and
                    texture_found == True and
                    texture_parameters_changed == False):
                    print "Skipping ", texture_display_name, " ... ", "ok"
                    continue # no need to update texture
            except:
                pass

        def checked_os_system(command):
            #print command
            output_file_name = os.path.join(output_dir, "nvdxt.output")
            cmd_result = os.system(command + " >\"" + output_file_name + "\"")
            if cmd_result != 0:
                print "FAILED\n"
                print "EXIT CODE: %d" % cmd_result
                print ""
                try:
                    output = file(output_file_name, "r").readlines()
                    os.remove(output_file_name)
                    if output != []:
                        print "COMMAND OUTPUT:"
                        sys.stdout.writelines(output)
                        print ""
                except:
                    pass
                sys.exit(2)
            os.remove(output_file_name)
#             in_out_err = os.popen3(command)
#             stdout = in_out_err[1].readlines()
#             stderr = in_out_err[2].readlines()
#             if stderr != [] or stdout != []:
#                 print "FAILED\n"
#                 print "ERROR IN COMMAND:"
#                 print command
#                 print ""
#                 #print "exit code: %d" % cmd_result
#                 print "ERROR OUTPUT:"
#                 sys.stdout.writelines(stderr)
#                 print ""
#                 if stdout != []:
#                     print "COMMAND OUTPUT:"
#                     sys.stdout.writelines(stdout)
#                     print ""
#                 sys.exit(2)

        def run_nvdxt(ifile, ofile, options):
            if os.path.splitext(ifile)[1].lower() == ".tga":
                swaprgb = "-swap " # nvdxt thinks that tga files
                                   # are in BGR format
            else:
                swaprgb = ""
            command = ("nvdxt"
                       + " -file \"" + ifile + "\""
                       + " -output \"" + ofile + "\""
                       + " -flip " # we need to flip texture for OpenGL
                       + " -quality_production " # best quality
                       + (" -clampScale %s, %s " % (max_width, max_height))
                       + (" -rescale nearest ")
                       #+ (" -rescale hi ")
                       #+ " -RescaleCubic " # rescaling as in photoshop
                       + " -RescaleBox " 
                       + " -Box " # more sharp details on mipmaps
                       #+ " -sharpenMethod ContrastMore " # too sharp
                       + swaprgb
                       + options # compression                       
                      )
            checked_os_system(command)
            
        # check compression is one of nvdxt support
        nvdxt_args = get_nvdxt_compression(compression)        
        if nvdxt_args != None:
            print "Exporting", texture_display_name, " ... ",
            run_nvdxt(texture, target_texture, nvdxt_args)
            print "ok"

        # check for osgCalNMExport compression
        nmexport_args = get_nmexport_compression(compression)
        if nmexport_args != None:
            def run_nmexport(ifile):
                command = ("osgCalNMExport"
                           + " -i \"" + ifile + "\""
                           + " -o \"" + target_texture + "\""
                           + (" -maxw %s" % max_width)
                           + (" -maxh %s" % max_height)
                           + nmexport_args)
                checked_os_system(command)
                    
            print "Exporting", texture_display_name, " ... ",
            # check for bump texture
            if type_of_texture == "bump":
                temp_normals_texture = os.path.join(output_dir, "tmp.nm.dds")
                run_nvdxt(texture, temp_normals_texture,
                #          "-n4 -u888 -rgb -nomipmap")
                          "-n4 -u888 -rgb -inputScale 5.0 5.0 5.0 5.0 -nomipmap")
                # We scale bump by 5 as in NVidia DDS Phothoshop plugin
                # and don't use -n3x3, 5x5, 7x7 or 9x9 filters since they
                # make bump look too smooth in comparison with one rendered in 3DSMax.
                run_nmexport(temp_normals_texture)
                os.remove(temp_normals_texture)
            else:
                run_nmexport(texture)                
            print "ok"

        # save new current textures list at the end of each step
        write_textures_list(current_textures_fname,
                            current_textures)

    # Finally delete other textures if necessary
    if delete_other_textures:
        new_current_textures = current_textures
        for ctexture, chash, ccompression, cmax_width, cmax_height in current_textures:
            texture_found = False
            for texture, hash, compression, max_width, max_height in textures_list:
                if texture == ctexture:
                    texture_found = True
                    break
            if texture_found == False:
                # remove unused texture
                target_texture = os.path.join(output_dir,
                                              dds_texture_name(ctexture))
                try:
                    print "Removing ", target_texture, " ... ",
                    os.remove(target_texture)
                    print "ok"
                except:
                    print "Can't remove", target_texture
                    sys.exit(2)
                # update current textures file
                new_current_textures = filter(lambda td: td[0] != ctexture,
                                              new_current_textures)
                write_textures_list(current_textures_fname,
                                    new_current_textures)       

def texture_name(texture):
    prefix_and_suffix = texture.split(":")
    if len(prefix_and_suffix) == 1:
        return texture
    else:
        return prefix_and_suffix[1]

def texture_type(texture):
    prefix_and_suffix = texture.split(":")
    if len(prefix_and_suffix) == 1:
        return ""
    else:
        return prefix_and_suffix[0]

def dds_texture_name(texture):
    basename = os.path.splitext(os.path.basename(texture_name(texture)))[0]
    if texture_type(texture) == "":
        return basename + ".dds"
    else:
        return basename + "." + texture_type(texture) + ".dds"
        # we add type as suffix to texture name

def get_nvdxt_compression(compression):
    "Return string of parameters to nvdxt tool which are specify compression"
    if compression == "DXT1":
        return "-dxt1c"
    if compression == "DXT3":
        return "-dxt3"
    if compression == "DXT5":
        return "-dxt5"
    if compression == "RGB8":
        return "-u888"
    if compression == "RGBA8":
        return "-u8888"
    if compression == "R5G6B5":
        return "-u565"
    if compression.startswith("DXT5_NM"):
        xscale = 1.0
        yscale = 1.0
        if compression.find("-flipx true") != -1:
            xscale = -1.0
        if compression.find("-flipy true") != -1:
            yscale = -1.0
        if xscale != 1.0 or yscale != 1.0:
            return "-dxt5nm -inputScale % % 1.0 1.0" % (xscale, yscale)
        else:
            return "-dxt5nm"
    return None # unknown compression

def get_nmexport_compression(compression):
    if compression.startswith("NormalsMap"):
        return compression[len("NormalsMap"):]
    if compression == "Bump":
        return "" # we convers bump maps to normal map using nvdxt
                  # and after this we use NMExport to generate A8L8 textures
    else:
        return None

def find_file_in_paths_list(file_name, paths_list):
    for p in paths_list:
        fn = os.path.join(p, file_name)
        if os.path.exists(fn):
            return fn
    return None

def rmnl(string):
    if string[len(string)-1] == '\n':
        return string[0:len(string)-1]
    else:
        return string


def read_textures_list(file_name):
    def add_md5_if_necessary(a):
        # source textures description doesn't keep MD5 hashes
        if len(a) == 4:
            a.insert(1, "") # empty MD5 hash
        return a
    f = file(file_name, "r")
    return map(lambda line: add_md5_if_necessary(rmnl(line).split(";")), f.readlines())

def write_textures_list(file_name, tl):
    f = file(file_name, "w")
    f.writelines([string.join(texdesc, ";")+"\n" for texdesc in tl])
    f.close()

def read_paths_list(file_name):
    f = file(file_name, "r")
    return map(rmnl, f.readlines())

def file_hash(file_name):
    f = file(file_name, "rb")
    m = md5.new()
    m.update(f.read())
    f.close()
    return binascii.hexlify(m.digest())

if __name__ == "__main__":
    main()
