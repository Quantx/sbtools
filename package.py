#!/usr/bin/python3

import os
import sys
import subprocess
import shutil

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

def hbx_to_gltf(hbxid):
    outid = hbxid
    
    if hbxid < 176: # Object
        if hbxid > 47: outid += 1 # E17
        if hbxid > 49: outid += 4 # E20, E21, E22, E23
        if hbxid > 55: outid += 2 # E30, E31
        if hbxid > 56: outid += 1 # E33
        
        if hbxid > 147: outid += 1 # H17 (There is no H16)
        
        if hbxid > 162: outid += 5 # J4, J5, K1, K2, K3
        
        outid = (outid * 2) + 806
    elif hbxid < 211: # VT
        outid = (outid - 176) * 8
    else: # Weapon
        if hbxid > 232: outid += 5 # MWEP22 - MWEP25, SWEP00
        
        if hbxid > 234: outid += 5 # SWEP03 - SWEP07
        if hbxid > 236: outid += 4 # SWEP10 - SWEP13
        if hbxid > 240: outid += 2 # SWEP18, SWEP 19
        if hbxid > 244: outid += 2 # SWEP24, SWEP 25
        
        outid = ((outid - 211) * 2) + 642

    return outid

def main(godot_path, root_path):
    if not os.path.isfile(godot_path):
        print("Could not locate Godot executable at path:", godot_path)
        sys.exit(1)

    BIN_PATHS = {}
    for b in ["ATARI", "MODEL", "MOTION", "TEXTURE", "VTMODEL"]:
        BIN_PATHS[b] = os.path.join(root_path, "media", "bin", b)

    TERRAIN_PATH = os.path.join(root_path, "media", "BumpData")

    COCKPIT_PATH = os.path.join(root_path, "media", "cockpit")
    
    ENGDATA_PATH = os.path.join(root_path, "media", "Eng_data")
    
    SOUND_PATH = os.path.join(root_path, "media", "sndeff")

    WEAPDATA_PATH = os.path.join(root_path, "media", "weapon")
    
    EFFECT_PATH = os.path.join(root_path, "media", "effdata")

    # Unpack the XBE
    XBE_PATH = os.path.join(root_path, "default")
    res = subprocess.run([tool_path("segment"), "-u", os.path.join(root_path, "default.xbe")])
    if res.returncode != 0: sys.exit(1)

    # Move segXX files to stage data folder
    STAGE_PATH = os.path.join(root_path, "media", "StgData")
    for i in range(0, 31, 1):
        os.replace(os.path.join(XBE_PATH, f"seg{i:02}.seg"), os.path.join(STAGE_PATH, f"seg{i:02}.seg"))
    
    # Move .data segment to stage data folder
    shutil.copy(os.path.join(XBE_PATH, f".data.seg"), os.path.join(STAGE_PATH, f".data.seg"))
    shutil.copy(os.path.join(XBE_PATH, f".data.hdr"), os.path.join(STAGE_PATH, f".data.hdr"))
    
    # Copy .data segment to Eng_data folder
    shutil.copy(os.path.join(XBE_PATH, f".data.seg"), os.path.join(ENGDATA_PATH, f".data.seg"))
    shutil.copy(os.path.join(XBE_PATH, f".data.hdr"), os.path.join(ENGDATA_PATH, f".data.hdr"))

    # Copy .data segment to Weapon folder
    shutil.copy(os.path.join(XBE_PATH, f".data.seg"), os.path.join(WEAPDATA_PATH, f".data.seg"))
    shutil.copy(os.path.join(XBE_PATH, f".data.hdr"), os.path.join(WEAPDATA_PATH, f".data.hdr"))
    
    # Copy .rdata segment to Weapon folder
    shutil.copy(os.path.join(XBE_PATH, f".rdata.seg"), os.path.join(WEAPDATA_PATH, f".rdata.seg"))
    shutil.copy(os.path.join(XBE_PATH, f".rdata.hdr"), os.path.join(WEAPDATA_PATH, f".rdata.hdr"))

    # Unpack bins
    for bin_name, bin_path in BIN_PATHS.items():
        res = subprocess.run([tool_path("binarize"), "-u", bin_path + ".bin"])
        if res.returncode != 0: sys.exit(1)

    # Convert Textures
    for tex in os.listdir(BIN_PATHS["TEXTURE"]):
        tex_name, ext = os.path.splitext(tex)
        tex_path = os.path.join(BIN_PATHS["TEXTURE"], tex)
        if ext == ".xpr":
            print("Converting texture:", tex_path)
            args = [tool_path("sbtexture"), "-d", tex_path]

            res = subprocess.run(args)
            if res.returncode != 0: sys.exit(1)

    # Convert Models
    for model in os.listdir(BIN_PATHS["MODEL"]):
        _, ext = os.path.splitext(model)
        model_path = os.path.join(BIN_PATHS["MODEL"], model)
        if ext == ".xbo":
            print("Converting model:", model_path)
            res = subprocess.run([tool_path("sbmodel"), model_path, "--flip"])
            if res.returncode != 0: sys.exit(1)

    # Convert VT Models
    for model in os.listdir(BIN_PATHS["VTMODEL"]):
        _, ext = os.path.splitext(model)
        model_path = os.path.join(BIN_PATHS["VTMODEL"], model)
        if ext == ".xbo":
            print("Converting model:", model_path)
            res = subprocess.run([tool_path("sbmodel"), model_path, "--flip"])
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

    # Weapon Animations
    for i, gltf in enumerate([706, 710, 696, 698, 744, 746, 748, 756, 680]):
        lmt = i + 11
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
        print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
        res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
        if res.returncode != 0: sys.exit(1)
    
    # VT Animations
    for lmt, vtl in VT_ANIMS.items():
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        for vt in vtl:
            vt -= 1
            for i in range(0, 8, 2):
                gltf = (vt * 8) + i
                gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")                
                
                print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
                res = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path, "--mirror"])
                if res.returncode != 0: sys.exit(1)
    
    # VT Hatch Animations
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

    # Cockpit Animations
    for r in [(90, 1237, 33), (123, 1271, 9), (132, 1281, 14), (146, 1296, 13), (159, 1310, 11), (170, 1322, 2)]:
        for i in range(0, r[2], 1):
            lmt = i + r[0]
            lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
            
            gltf = i + r[1]
            gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
            
            print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
            subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
    
    # Cockpit lighting
    subprocess.run([tool_path("sbcockpit"), os.path.join(COCKPIT_PATH, "AMBPACK.amb")])
    subprocess.run([tool_path("sbcockpit"), os.path.join(COCKPIT_PATH, "PLPACK.coc" )])
    subprocess.run([tool_path("sbcockpit"), os.path.join(COCKPIT_PATH, "BTLPACK.cbt")])

    # Map Object Animations
    for i, gltf in enumerate([902, 906, 916, 928, 930, 932, 962]):
        lmt = i + 172
        lmt_path = os.path.join(BIN_PATHS["MOTION"], f"{lmt:04}.lmt")
        
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")
        
        print("Adding motion file:", lmt_path, "To glTF file:", gltf_path)
        ret = subprocess.run([tool_path("sbmotion"), lmt_path, gltf_path])
        if res.returncode != 0: sys.exit(1)

    # Convert Hitboxes
    for hbxid in range(0, 251):
        if hbxid == 180: continue # VT05
        if hbxid > 207 and hbxid < 211: continue # VT33 - VT35
        
        hbx_path = os.path.join(BIN_PATHS["ATARI"], f"{hbxid:04}.ppd")

        gltf = hbx_to_gltf(hbxid)
        gltf_path = os.path.join(BIN_PATHS["MODEL"], f"{gltf:04}.gltf")

        print("Converting hitbox:", hbx_path)
        res = subprocess.run([tool_path("sbhitbox"), hbx_path, gltf_path])
        if res.returncode != 0: sys.exit(1)

    # Convert Terrains
    for terr in os.listdir(TERRAIN_PATH):
        _, ext = os.path.splitext(terr)
        terr_path = os.path.join(TERRAIN_PATH, terr)
        if ext == ".gnd":
            print("Converting terrain:", terr_path)
            res = subprocess.run([tool_path("sbterrain"), "-u", terr_path])
            if res.returncode != 0: sys.exit(1)

    # Convert sounds
    res = subprocess.run([tool_path("sbsound"), os.path.join(SOUND_PATH, "Bank.xsb")])
    if res.returncode != 0: sys.exit(1)

    # Unpack effects
    res = subprocess.run([tool_path("sbeffect"), os.path.join(EFFECT_PATH, "effect.efp")])
    if res.returncode != 0: sys.exit(1)
    
    # Convert effects
    for effect in os.listdir(EFFECT_PATH):
        _, ext = os.path.splitext(effect)
        eff_path = os.path.join(EFFECT_PATH, effect)
        if ext in [".efe", ".seq", ".uv2"]:
            res = subprocess.run([tool_path("sbeffect"), eff_path])
            if res.returncode != 0: sys.exit(1)
    
    # Extract engine effect data
    res = subprocess.run([tool_path("sbeffect"), os.path.join(XBE_PATH, ".data.seg")])
    if res.returncode != 0: sys.exit(1)

    # Create build directory
    out_path = os.path.join("build", "proprietary", "loc")
    if not os.path.isdir(out_path):
        os.makedirs(out_path)
    
    # Copy Godot data
    godot_presets = "export_presets.cfg"
    shutil.copy(godot_presets, os.path.join("build", godot_presets))
    
    # Touch the Godot project file, it has no contents
    with open(os.path.join("build", "project.godot"), "w") as file: pass
    
    # Extract strings
    res = subprocess.run([tool_path("sbtext"), os.path.join(XBE_PATH, ""), os.path.join(out_path, "strings.csv")])
    if res.returncode != 0: sys.exit(1)
    
    # Copy all map data
    STAGE_PATH = os.path.join(root_path, "media", "StgData", "") # The Extra string forces a trailing slash
    for i in range(0, 27, 1):
        mapid = f"map{i:02}"

        mission_path = os.path.join(out_path, "maps", mapid)
        if not os.path.isdir(mission_path):
            os.makedirs(mission_path)
        
        shutil.copy(os.path.join(TERRAIN_PATH, mapid + "hit.gad"), os.path.join(mission_path, "hit.gad"))
        
        try:
            os.replace(os.path.join(TERRAIN_PATH, mapid + ".tga"), os.path.join(mission_path, "terrain.tga"))
        except FileNotFoundError:
            pass
        
        try:
            os.replace(os.path.join(TERRAIN_PATH, mapid + ".dds"), os.path.join(mission_path, "height.dds"))
        except FileNotFoundError:
            pass

        objtex = i + 157
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{objtex:04}.dds"), os.path.join(mission_path, "object.dds"))
        
        gndtex = i + 184
        try:
            os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{gndtex:04}.dds"), os.path.join(mission_path, "ground.dds"))
        except FileNotFoundError:
            pass
        
        maptex = i + 211
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{maptex:04}.dds"), os.path.join(mission_path, "map.dds"))
        
        mapsmalltex = i + 238
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{mapsmalltex:04}.dds"), os.path.join(mission_path, "map_small.dds"))
        
        skytex0 = (i * 2) + 265
        try:
            os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{skytex0:04}.dds"), os.path.join(mission_path, "sky0.dds"))
        except FileNotFoundError:
            pass
        
        skytex1 = (i * 2) + 266
        try:
            if skytex1 == 316: # Reuse skytex1 for mission 25 and 26
                shutil.copy(os.path.join(BIN_PATHS["TEXTURE"], "0316.dds"), os.path.join(mission_path, "sky1.dds"))
            else:
                if skytex1 == 318:
                    skytex1 = 316
                
                os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{skytex1:04}.dds"), os.path.join(mission_path, "sky1.dds"))
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

    # Copy map hitboxes
    for i in range(0, 176):
        gltf = hbx_to_gltf(i)
        os.replace(os.path.join(BIN_PATHS["ATARI"], f"{i:04}.hbx"), os.path.join(mapobj_path, f"{gltf:04}.hbx"))

    # Copy cockpits
    cockpit_base_path = os.path.join(out_path, "cockpit")
    cockpit_objs = [1238, 1256, 1271, 1284, 1298, 1310, 1322]
    for i, name in enumerate(["gen1", "gen2", "gen3", "jar", "gen1s", "gen2s"]):
        cockpit_path = os.path.join(cockpit_base_path, name)
        if not os.path.isdir(cockpit_path):
            os.makedirs(cockpit_path)
    
        texid = i

        # Gen 2 / Jar have swapped textures
        if i == 3:
            texid = 5
        elif i == 5:
            texid = 3

        uitex = texid + 126
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{uitex:04}.dds"), os.path.join(cockpit_path, "ui.dds"))
        
        tex0 = (texid * 2) + 133
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{tex0:04}.dds"), os.path.join(cockpit_path, "texture0.dds"))
        
        tex1 = (texid * 2) + 134
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{tex1:04}.dds"), os.path.join(cockpit_path, "texture1.dds"))
        
        disp0 = (texid * 2) + 145
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{disp0:04}.dds"), os.path.join(cockpit_path, "display0.dds"))
        
        disp1 = (texid * 2) + 146
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{disp1:04}.dds"), os.path.join(cockpit_path, "display1.dds"))
    
        objstart = cockpit_objs[i]
        objend = cockpit_objs[i + 1]
        for obj in range(objstart, objend, 1):
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{obj:04}.gltf"),  os.path.join(cockpit_path, f"{obj:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{obj:04}.glbin"), os.path.join(cockpit_path, f"{obj:04}.glbin"))
        
        # Copy boot lighting data
        for j, offset in enumerate([0, 1, 2, 21, 22, 23, 24]):
            btId = i * 3 + offset
            # Use a copy here to fix a problem where 21 and 24 overlap
            shutil.copy(os.path.join(COCKPIT_PATH, f"BTLIG_{btId:02}.json"), os.path.join(cockpit_path, f"BTLIG_{j}.json"))
            shutil.copy(os.path.join(COCKPIT_PATH, f"BTAMB_{btId:02}.json"), os.path.join(cockpit_path, f"BTAMB_{j}.json"))
        
        # Copy ambient and dynamic lights
        lig0 = i
        os.replace(os.path.join(COCKPIT_PATH, f"PLIG_{lig0:02}.json"), os.path.join(cockpit_path, "PLIG_0.json"))
        os.replace(os.path.join(COCKPIT_PATH,  f"AMB_{lig0:02}.json"), os.path.join(cockpit_path, "AMB_0.json"))
        
        lig1 = i + 6
        os.replace(os.path.join(COCKPIT_PATH, f"PLIG_{lig1:02}.json"), os.path.join(cockpit_path, "PLIG_1.json"))
        os.replace(os.path.join(COCKPIT_PATH,  f"AMB_{lig1:02}.json"), os.path.join(cockpit_path, "AMB_1.json"))
    
    # Copy cockpit ejection bar
    os.replace(os.path.join(BIN_PATHS["MODEL"], "1323.gltf"),  os.path.join(cockpit_base_path, "1323.gltf"))
    os.replace(os.path.join(BIN_PATHS["MODEL"], "1323.glbin"), os.path.join(cockpit_base_path, "1323.glbin"))

    # Copy cockpit strings
    shutil.copy(os.path.join(COCKPIT_PATH, "os.str"), os.path.join(cockpit_base_path, "os.txt"))

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

        hbxid = i + 176
        os.replace(os.path.join(BIN_PATHS["ATARI"], f"{hbxid:04}.hbx"), os.path.join(mech_path, "hitbox.hbx"))
    
        for j in range(0, 8, 2):
            vtid = (i * 8) + j
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtid:04}.gltf"),  os.path.join(mech_path, f"{vtid:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vtid:04}.glbin"), os.path.join(mech_path, f"{vtid:04}.glbin"))
            
            vthid = vtid + 280
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vthid:04}.gltf"),  os.path.join(mech_path, f"{vthid:04}.gltf"))
            os.replace(os.path.join(BIN_PATHS["MODEL"], f"{vthid:04}.glbin"), os.path.join(mech_path, f"{vthid:04}.glbin"))
    
    # Copy engine data
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
    
    # Copy bullet models
    for i in range(1186, 1216, 2):
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.gltf"),  os.path.join(weapon_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.glbin"), os.path.join(weapon_path, f"{i:04}.glbin"))
    
    # Copy weapon hitboxes
    for hbxid in range(211, 251):
        gltf = hbx_to_gltf(hbxid)
        os.replace(os.path.join(BIN_PATHS["ATARI"], f"{hbxid:04}.hbx"), os.path.join(weapon_path, f"{gltf:04}.hbx"))
    
    # Copy weapon data
    res = subprocess.run([tool_path("sbweapon"), os.path.join(WEAPDATA_PATH, "wepdat.wcb")])
    if res.returncode != 0: sys.exit(1)
    os.replace(os.path.join(WEAPDATA_PATH, "weapondata.json"), os.path.join(weapon_path, "weapondata.json"))
    
    # Copy portraits
    portrait_path = os.path.join(out_path, "portrait")
    if not os.path.isdir(portrait_path):
            os.makedirs(portrait_path)
    for i in range(0, 105, 1):
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{i:04}.dds"), os.path.join(portrait_path, f"{i:03}.dds"))
        
    # Copy menu models
    menu_path = os.path.join(out_path, "menu")
    if not os.path.isdir(menu_path):
            os.makedirs(menu_path)
    for i in range(758, 764, 2):
        os.replace(os.path.join(BIN_PATHS["VTMODEL"], f"{i:04}.gltf"),  os.path.join(menu_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["VTMODEL"], f"{i:04}.glbin"), os.path.join(menu_path, f"{i:04}.glbin"))
    
    # Copy menu textures
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0106.dds"), os.path.join(menu_path, "online.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0115.dds"), os.path.join(menu_path, "object.dds"))
    
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0319.dds"), os.path.join(menu_path, "company0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0320.dds"), os.path.join(menu_path, "company1.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0321.dds"), os.path.join(menu_path, "company2.dds"))

    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0323.dds"), os.path.join(menu_path, "victory.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0324.dds"), os.path.join(menu_path, "defeat.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0325.dds"), os.path.join(menu_path, "escape.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0326.dds"), os.path.join(menu_path, "death.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0327.dds"), os.path.join(menu_path, "promotion.dds"))

    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0338.dds"), os.path.join(menu_path, "screenshot0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0339.dds"), os.path.join(menu_path, "screenshot1.dds"))
    
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0340.dds"), os.path.join(menu_path, "splashscreen0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0341.dds"), os.path.join(menu_path, "splashscreen1.dds"))
    
    # Copy effect textures
    effect_path = os.path.join(out_path, "effects")
    if not os.path.isdir(effect_path):
        os.makedirs(effect_path)
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0105.dds"), os.path.join(effect_path, "smallfont.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0107.dds"), os.path.join(effect_path, "spritesheet0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0108.dds"), os.path.join(effect_path, "spritesheet1.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0109.dds"), os.path.join(effect_path, "scanlines.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0110.dds"), os.path.join(effect_path, "loading.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0111.dds"), os.path.join(effect_path, "flash.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0112.dds"), os.path.join(effect_path, "palette.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0113.dds"), os.path.join(effect_path, "controller0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0114.dds"), os.path.join(effect_path, "controller1.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0132.dds"), os.path.join(effect_path, "uisheet0.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0318.dds"), os.path.join(effect_path, "uisheet1.dds"))
    os.replace(os.path.join(BIN_PATHS["TEXTURE"], "0322.dds"), os.path.join(effect_path, "eject.dds"))
    
    # Copy effect table
    shutil.copy(os.path.join(EFFECT_PATH, "effect.tbl"), os.path.join(effect_path, "effect_ids.data"))
    
    # Copy effect light data
    os.replace(os.path.join(XBE_PATH, "lightdata.json"), os.path.join(effect_path, "lightdata.json"))
    
    # Copy effect data
    for effect in os.listdir(EFFECT_PATH):
        _, ext = os.path.splitext(effect)
        eff_path = os.path.join(EFFECT_PATH, effect)
        if ext == ".json":
            os.replace(eff_path, os.path.join(effect_path, effect))
    
    # Copy effect models
    for i in range(1216, 1236, 2):
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.gltf"),  os.path.join(effect_path, f"{i:04}.gltf"))
        os.replace(os.path.join(BIN_PATHS["MODEL"], f"{i:04}.glbin"), os.path.join(effect_path, f"{i:04}.glbin"))
    
    # Copy water textures
    water_path = os.path.join(out_path, "water")
    if not os.path.isdir(water_path):
        os.makedirs(water_path)
    for i in range(0, 10):
        waterid = 116 + i
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{waterid:04}.dds"), os.path.join(water_path, f"water{i}.dds"))

    # Copy Emblems
    emblem_path = os.path.join(out_path, "emblems")
    if not os.path.isdir(emblem_path):
        os.makedirs(emblem_path)
    for i in range(0, 10, 1):
        emblem = i + 328
        os.replace(os.path.join(BIN_PATHS["TEXTURE"], f"{emblem:04}.dds"), os.path.join(emblem_path, f"{i}.dds"))

    # Build Godot package
    res = subprocess.run([godot_path, "--headless", "--path", "build", "--export-pack", "Proprietary", os.path.join("..", "Proprietary.pck")])
    if res.returncode != 0:
        print("Failed to build Godot package")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: ./package.py <path_to_godot> <path_to_game_directory>")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
