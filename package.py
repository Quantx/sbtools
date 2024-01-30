#!/usr/bin/python3

import os
import sys
import subprocess

WINDOWS = os.name == 'nt'

VT_ANIMS = {
    # Remember VT05 doesn't exist
    20: [1, 2], # VT01.LMT : Bone count 27
    23: [4, 16], # VT04.LMT : Bone count 28
    26: [7, 15, 31], # VT07.LMT : Bone count 27
    27: [8, 6, 27, 28, 32], # VT08.LMT : Bone count 31
    28: [9, 10, 3, 23], # VT09.LMT : Bone count 31
    30: [11, 12, 13], # VT11.LMT : Bone count 39
    38: [19, 17, 18, 26], # VT19.LMT : Bone count 29
    39: [20, 21], # VT20.LMT : Bone count 28
    41: [22], # VT22.LMT : Bone count 25
    43: [24], # VT24.LMT : Bone count 30
    44: [25, 14], # VT25.LMT : Bone count 29
    48: [29], # VT29.LMT : Bone count 27
    49: [30], # VT30.LMT : Bone count 27
}

VH_ANIMS = {
    # Remember VH05 doesn't exist
    55 : [1, 2], # VH01.LMT : Bone count 2
    57 : [3], # VH03.LMT : Bone count 2
    58 : [4, 16], # VH04.LMT : Bone count 4
    60 : [6], # VH06.LMT : Bone count 2
    61 : [7], # VH07.LMT : Bone count 3
    62 : [8], # VH08.LMT : Bone count 2
    63 : [9, 10], # VH09.LMT : Bone count 2
    65 : [11, 12], # VH11.LMT : Bone count 4
    67 : [13], # VH13.LMT : Bone count 4
    68 : [14], # VH14.LMT : Bone count 4
    69 : [15], # VH15.LMT : Bone count 3
    73 : [19, 17, 18], # VH19.LMT : Bone count 2
    74 : [20, 21], # VH20.LMT : Bone count 4
    76 : [22], # VH22.LMT : Bone count 3
    77 : [23], # VH23.LMT : Bone count 4
    78 : [24, 32], # VH24.LMT : Bone count 3
    79 : [25], # VH25.LMT : Bone count 5
    80 : [26], # VH26.LMT : Bone count 5
    81 : [27], # VH27.LMT : Bone count 3
    82 : [28], # VH28.LMT : Bone count 2
    83 : [29], # VH29.LMT : Bone count 4
    84 : [30], # VH30.LMT : Bone count 7
    85 : [31], # VH31.LMT : Bone count 4
}

def tool_path(tool):
    if WINDOWS:        
        return os.path.join("windows", tool + ".exe")
    return os.path.join("linux", tool)

def main(root_path, out_path):
    BIN_PATHS = {}
    for b in ["ATARI", "MODEL", "MOTION", "TEXTURE", "VTMODEL"]:
        BIN_PATHS[b] = os.path.join(root_path, "media", "bin", b)

    TERRAIN_PATH = os.path.join(root_path, "media", "BumpData")

    # Unpack the XBE
    XBE_PATH = os.path.join(root_path, "default")
    res = subprocess.run([tool_path("segment"), "-u", os.path.join(root_path, "default.xbe")])
    if res.returncode != 0: sys.exit(1)

    # Move segXX files to stage data folder
    STAGE_PATH = os.path.join(root_path, "media", "StgData")
    for i in range(0, 31, 1):
        os.replace(os.path.join(XBE_PATH, f"seg{i:02}.seg"), os.path.join(STAGE_PATH, f"seg{i:02}.seg"))

    # Unpack bins
    for bin_name, bin_path in BIN_PATHS.items():
        res = subprocess.run([tool_path("binarize"), "-u", bin_path + ".bin"])
        if res.returncode != 0: sys.exit(1)

    # Convert Textures
    for tex in os.listdir(BIN_PATHS["TEXTURE"]):
        _, ext = os.path.splitext(tex)
        tex_path = os.path.join(BIN_PATHS["TEXTURE"], tex)
        if ext == ".xpr":
            print("Converting texture:", tex_path)
            res = subprocess.run([tool_path("sbtexture"), tex_path])
            if res.returncode != 0: sys.exit(1)

    # Convert Models
    for model in os.listdir(BIN_PATHS["MODEL"]):
        _, ext = os.path.splitext(model)
        model_path = os.path.join(BIN_PATHS["MODEL"], model)
        if ext == ".xbo":
            print("Converting model:", model_path)
            res = subprocess.run([tool_path("sbmodel"), model_path])
            if res.returncode != 0: sys.exit(1)

    # Convert VT Models
    for model in os.listdir(BIN_PATHS["VTMODEL"]):
        _, ext = os.path.splitext(model)
        model_path = os.path.join(BIN_PATHS["VTMODEL"], model)
        if ext == ".xbo":
            print("Converting model:", model_path)
            res = subprocess.run([tool_path("sbmodel"), model_path])
            if res.returncode != 0: sys.exit(1)

    # Apply animations
    for i in range(0, 10, 1):
        lmt = i + 1
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        gltf = (i * 2) + 622
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
        print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
        res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
        if res.returncode != 0: sys.exit(1)

    for i, gltf in enumerate([706, 710, 696, 698, 744, 746, 748, 756, 680]):
        lmt = i + 11
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
        print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
        res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
        if res.returncode != 0: sys.exit(1)
    
    # VTs
    for lmt, vtl in VT_ANIMS.items():
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        for vt in vtl:
            vt -= 1
            for i in range(0, 8, 2):
                gltf = (vt * 8) + i
                gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")                
                
                print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
                res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
                if res.returncode != 0: sys.exit(1)
    
    # VT Hatches
    for lmt, vtl in VH_ANIMS.items():
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        for vt in vtl:
            vt -= 1
            for i in range(0, 8, 2):
                gltf = 280 + (vt * 8) + i
                gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")                
                
                print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
                res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
                if res.returncode != 0: sys.exit(1)

    # Cockpit
    for r in [(90, 1237, 33), (123, 1271, 36), (159, 1310, 11), (170, 1322, 2)]:
        for i in range(0, r[2], 1):
            lmt = i + r[0]
            lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
            
            gltf = i + r[1]
            gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
            
            print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
            subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])

    # Map objects
    for i, gltf in enumerate([902, 906, 916, 928, 930, 932, 962]):
        lmt = i + 172
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
        
        print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
        ret = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
        if res.returncode != 0: sys.exit(1)

    # Convert Terrains
    for terr in os.listdir(TERRAIN_PATH):
        terr_base, ext = os.path.splitext(terr)
        terr_path = os.path.join(TERRAIN_PATH, terr_base)
        if terr_path.endswith("hit"):
            terr_path = terr_path[:-3]
        if ext == ".gad":
            print("Converting terrain:", terr_path)
            res = subprocess.run([tool_path("sbterrain"), "-u", terr_path])
            if res.returncode != 0: sys.exit(1)

    # Copy all map data
    STAGE_PATH = os.path.join(root_path, "media", "StgData", "") # The Extra string forces a trailing slash
    for i in range(0, 25, 1):
        mapid = f"map{i:02}"

        mission_path = os.path.join(out_path, "maps", mapid)
        if not os.path.isdir(mission_path):
            os.makedirs(mission_path)
        
        os.replace(os.path.join(TERRAIN_PATH, mapid + "_hit.tga"), os.path.join(mission_path, "hit.tga"))
        
        try:
            os.replace(os.path.join(TERRAIN_PATH, mapid + "_texture.tga"), os.path.join(mission_path, "terrain.tga"))
        except FileNotFoundError:
            pass
            
        try:
            os.replace(os.path.join(TERRAIN_PATH, mapid + "_height.data"), os.path.join(mission_path, "height.data"))
        except FileNotFoundError:
            pass

        objtex = i + 157
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{objtex:04}.tga"), os.path.join(mission_path, "object.tga"))
        
        gndtex = i + 184
        try:
            os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{gndtex:04}.tga"), os.path.join(mission_path, "ground.tga"))
        except FileNotFoundError:
            pass
        
        maptex = i + 211
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{maptex:04}.tga"), os.path.join(mission_path, "map.tga"))
        
        skytex0 = (i * 2) + 265
        try:
            os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{skytex0:04}.tga"), os.path.join(mission_path, "sky0.tga"))
        except FileNotFoundError:
            pass
        
        skytex1 = (i * 2) + 266
        try:
            os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{skytex1:04}.tga"), os.path.join(mission_path, "sky1.tga"))
        except FileNotFoundError:
            pass
            
        res = subprocess.run([tool_path("sbstage"), str(i), STAGE_PATH])
        if res.returncode != 0: sys.exit(1)

        os.replace(os.path.join(STAGE_PATH, mapid + ".json"), os.path.join(mission_path, "config.json"))

    # Copy map objects
    mapobj_path = os.path.join(out_path, "objects")
    if not os.path.isdir(mapobj_path):
        os.makedirs(mapobj_path)
    for i in range(798, 1186, 2):
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.gltf"), os.path.join(mapobj_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.glbin"), os.path.join(mapobj_path, f"{i:04}.glbin"))

    # Copy cockpits
    cockpit_base_path = os.path.join(out_path, "cockpit")
    cockpit_objs = [1238, 1256, 1271, 1284, 1298, 1310, 1322]
    for i, name in enumerate(["gen1", "gen2", "gen3", "gen2s", "gen1s", "jar"]):
        cockpit_path = os.path.join(cockpit_base_path, name)
        if not os.path.isdir(cockpit_path):
            os.makedirs(cockpit_path)
    
        uitex = i + 126
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{uitex:04}.tga"), os.path.join(cockpit_path, "ui.tga"))
        
        tex0 = (i * 2) + 133
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{tex0:04}.tga"), os.path.join(cockpit_path, "texture0.tga"))
        
        tex1 = (i * 2) + 134
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{tex1:04}.tga"), os.path.join(cockpit_path, "texture1.tga"))
        
        disp0 = (i * 2) + 145
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{disp0:04}.tga"), os.path.join(cockpit_path, "display0.tga"))
        
        disp1 = (i * 2) + 146
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{disp1:04}.tga"), os.path.join(cockpit_path, "display1.tga"))
    
        objstart = cockpit_objs[i]
        objend = cockpit_objs[i + 1]
        for obj in range(objstart, objend, 1):
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{obj:04}.gltf"),  os.path.join(cockpit_path, f"{obj:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{obj:04}.glbin"), os.path.join(cockpit_path, f"{obj:04}.glbin"))
    
    # Copy cockpit ejection bar
    os.replace(os.path.join(BIN_PATHS["MODEL"], "1323.gltf"),  os.path.join(cockpit_base_path, "1323.gltf"))
    os.replace(os.path.join(BIN_PATHS["MODEL"], "1323.glbin"), os.path.join(cockpit_base_path, "1323.glbin"))

    # Copy VTs
    mech_base_path = os.path.join(out_path, "mechs")
    for i in range(0, 32, 1):
        if i == 4: continue # There's no VT here
        mech_path = os.path.join(mech_base_path, f"mech{i:02}")
        if not os.path.isdir(mech_path):
            os.makedirs(mech_path)
    
        vtmid = (i * 2) + 560
        if i > 4: vtmid -= 2
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtmid:04}.gltf"),  os.path.join(mech_path, f"{vtmid:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtmid:04}.glbin"), os.path.join(mech_path, f"{vtmid:04}.glbin"))
    
        for j in range(0, 8, 2):
            vtid = (i * 8) + j
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtid:04}.gltf"),  os.path.join(mech_path, f"{vtid:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtid:04}.glbin"), os.path.join(mech_path, f"{vtid:04}.glbin"))
            
            vthid = vtid + 280
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vthid:04}.gltf"),  os.path.join(mech_path, f"{vthid:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vthid:04}.glbin"), os.path.join(mech_path, f"{vthid:04}.glbin"))
    
    # Copy engine data
    ENGDATA_PATH = os.path.join(root_path, "media", "Eng_data")
    res = subprocess.run([tool_path("sbengine"), "-u", os.path.join(ENGDATA_PATH, "eng_data.eng")])
    if res.returncode != 0: sys.exit(1)
    os.replace(os.path.join(ENGDATA_PATH, "mechdata.json"), os.path.join(mech_base_path, "mechdata.json"))
    
    # Copy weapons
    weapon_path = os.path.join(out_path, "weapons")
    if not os.path.isdir(weapon_path):
        os.makedirs(weapon_path)
    for wepid in range(622, 758, 2):
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{wepid:04}.gltf"),  os.path.join(weapon_path, f"{wepid:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{wepid:04}.glbin"), os.path.join(weapon_path, f"{wepid:04}.glbin"))
    
    # Copy weapon data
    WEAPDATA_PATH = os.path.join(root_path, "media", "weapon")
    res = subprocess.run([tool_path("sbweapon"), os.path.join(WEAPDATA_PATH, "wepdat.wcb")])
    if res.returncode != 0: sys.exit(1)
    os.replace(os.path.join(WEAPDATA_PATH, "weapondata.json"), os.path.join(weapon_path, "weapondata.json"))
    
    # Copy portraits
    portrait_path = os.path.join(out_path, "portrait")
    if not os.path.isdir(portrait_path):
            os.makedirs(portrait_path)
    for i in range(0, 105, 1):
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{i:04}.tga"), os.path.join(portrait_path, f"{i:03}.tga"))
        
    # Copy menu models
    menu_path = os.path.join(out_path, "menu")
    if not os.path.isdir(menu_path):
            os.makedirs(menu_path)
    for i in range(758, 764, 2):
        os.replace(os.path.join(BIN_PATHS["VTMODEL"], f"{i:04}.gltf"),  os.path.join(menu_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["VTMODEL"], f"{i:04}.glbin"), os.path.join(menu_path, f"{i:04}.glbin"))
    
    # Copy menu textures
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0106.tga"), os.path.join(menu_path, "online.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0113.tga"), os.path.join(menu_path, "controller.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0115.tga"), os.path.join(menu_path, "texture.tga"))
    
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0319.tga"), os.path.join(menu_path, "company0.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0320.tga"), os.path.join(menu_path, "company1.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0321.tga"), os.path.join(menu_path, "company2.tga"))

    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0323.tga"), os.path.join(menu_path, "victory.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0324.tga"), os.path.join(menu_path, "defeat.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0325.tga"), os.path.join(menu_path, "escape.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0326.tga"), os.path.join(menu_path, "death.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0327.tga"), os.path.join(menu_path, "promotion.tga"))

    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0338.tga"), os.path.join(menu_path, "screenshot0.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0339.tga"), os.path.join(menu_path, "screenshot1.tga"))
    
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0340.tga"), os.path.join(menu_path, "splashscreen0.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0341.tga"), os.path.join(menu_path, "splashscreen1.tga"))
    
    # Copy effect textures
    effect_path = os.path.join(out_path, "effects")
    if not os.path.isdir(effect_path):
        os.makedirs(effect_path)
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0322.tga"), os.path.join(effect_path, "eject.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0107.tga"), os.path.join(effect_path, "spritesheet0.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0108.tga"), os.path.join(effect_path, "spritesheet1.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0111.tga"), os.path.join(effect_path, "sun.tga"))

    for i in range(1186, 1236, 2):
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.gltf"),  os.path.join(effect_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.glbin"), os.path.join(effect_path, f"{i:04}.glbin"))

    # Copy UI textures
    ui_path = os.path.join(out_path, "ui")
    if not os.path.isdir(ui_path):
        os.makedirs(ui_path)
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0114.tga"), os.path.join(ui_path, "controller.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0132.tga"), os.path.join(ui_path, "spritesheet0.tga"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0318.tga"), os.path.join(ui_path, "spritesheet1.tga"))

    # Copy Emblems
    emblem_path = os.path.join(out_path, "emblems")
    if not os.path.isdir(emblem_path):
        os.makedirs(emblem_path)
    for i in range(0, 10, 1):
        emblem = i + 328
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{emblem:04}.tga"), os.path.join(emblem_path, f"{i}.tga"))

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: ./package.py <path_to_game_directory> <output_directory>")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
