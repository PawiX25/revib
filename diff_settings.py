"""asm-differ's settings: python3 tools/asm-differ/diff.py [--exe EXE] <function>"""

# the original, the build, its map and its asm, for each executable
EXECUTABLES = {
    "title": ("iso/MAIN_T.EXE", "build/MAIN_T.EXE", "build/vib-ribbon.map", "asm/game"),
    "kiosk": ("iso/MAIN_K.EXE", "build/main_k/MAIN_K.EXE", "build/main_k/main_k.map", "asm/main_k"),
    "game": ("iso/MAIN_G.EXE", "build/main_g/MAIN_G.EXE", "build/main_g/main_g.map", "asm/main_g"),
    "loader": ("iso/SCES_028.73", "build/loader/SCES_028.73", "build/loader/loader.map", "asm/loader"),
}


def add_custom_arguments(parser):
    parser.add_argument("--exe", choices=EXECUTABLES, default="title",
                        help="the executable the function is in (default: title, MAIN_T.EXE)")


def apply(config, args):
    baseimg, myimg, mapfile, asm = EXECUTABLES[args.exe]
    config["arch"] = "mipsel"
    config["baseimg"] = baseimg
    config["myimg"] = myimg
    config["mapfile"] = mapfile
    config["source_directories"] = ["src/game", "include", asm]
