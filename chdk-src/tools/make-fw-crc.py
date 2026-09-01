#!/usr/bin/env python3

# This script generates checksums for use with the firmware_crc module
# use -h for command line syntax and usage

import os
import sys
import zlib
import io
import csv
import argparse

self_dir = os.path.dirname(os.path.abspath(__file__))
# make chdklib available with normal names
sys.path.append(os.path.join(self_dir,'ghidra_scripts'))

# to set verbosity
import chdklib.logutil
from chdklib.logutil import infomsg, warn
from chdklib.stubs_loader import StubsData

class ChecksumInfo:
    """Blocks to CRC and associated file info"""
    def __init__(self,filepath):
        self.blocks=[]
        self.filepath = filepath
        self.filesize = os.path.getsize(filepath)
        self.total_size = 0

    def add_block(self,blk):
        b = blk.copy()
        if b['offset'] >= self.filesize:
            warn("block %s start outside file, ignored"%(b['name']))
            b['size']=0

        elif self.filesize - b['offset'] < b['size']:
            warn("block %s end outside file, truncated"%(b['name']))
            b['size'] = self.filesize - b['offset']

        self.total_size += b['size']
        self.blocks.append(b)

    def merge_blocks(self):
        '''merge adjacent blocks to reduce CHDK core data'''
        blocks = []
        pb = None;
        for b in sorted(self.blocks,key=lambda b: b['offset']):
            if pb:
                infomsg(2,'%s %s %x\n'%(pb['name'],b['name'],b['offset']-(pb['offset']+pb['size'])))
                pb_end = pb['offset'] + pb['size']
                # should be handled correctly, but may indicate incorrect block detection
                if pb_end > b['offset']:
                    warn('overlapping blocks %s %s\n'%(pb['name'],b['name']))

                # allow a modest fudge, assuming Canon wouldn't put a varying chunk in a small space
                if b['offset'] <= pb['offset'] + pb['size'] + 32:
                    nb = pb
                    nb['name'] += ', '+ b['name']
                    nb['size'] += b['size'] + b['offset'] - (pb['offset'] + pb['size'])
                    pb = nb
                    continue
            pb = b
            blocks.append(b)

        self.blocks = blocks


    def process_stubs(self,stubs_data):
        smisc = stubs_data.stubs_entry_misc
        romcode_start = smisc.get('main_fw_start')
        # not default to get(), may be literal None
        if not romcode_start:
            romcode_start = smisc['ar_rom']['start_adr']

        rom_base = smisc['ar_rom']['start_adr']

        self.add_block({
            'name':'ROMCODE',
            'start_adr':romcode_start,
            'offset':romcode_start - rom_base,
            'size':smisc['main_fw_code_end'] - romcode_start + 1,
        })
        for ar_name in ('ar_ramcode','ar_btcmcode','ar_ramdata'):
            ar = smisc.get(ar_name)
            if not ar:
                continue

            self.add_block({
                'name':ar['name'],
                'start_adr':ar['src_adr'],
                'offset':ar['src_adr'] - rom_base,
                'size':ar['size'],
            })

        for ar in smisc['zico_blobs']:
            self.add_block({
                'name':ar['name'],
                'start_adr':ar['src_adr'],
                'offset':ar['src_adr'] - rom_base,
                'size':ar['size'],
            })

        for ar in smisc['omar_blobs']:
            self.add_block({
                'name':ar['name'],
                'start_adr':ar['src_adr'],
                'offset':ar['src_adr'] - rom_base,
                'size':ar['size'],
            })

        self.merge_blocks()

        self.crc32_all()

    def process_custom(self,stubs_data,custom_blocks):
        smisc = stubs_data.stubs_entry_misc
        romcode_start = smisc.get('main_fw_start')
        # not default to get(), may be literal None
        if not romcode_start:
            romcode_start = smisc['ar_rom']['start_adr']

        rom_base = smisc['ar_rom']['start_adr']

        for bd in custom_blocks:
            self.add_block({
                'name':bd['name'],
                'start_adr':bd['start_adr'],
                'offset':bd['start_adr'] - rom_base,
                'size':bd['end_adr'] - bd['start_adr'],
            })

        # don't merge, it ignores small spaces
        #self.merge_blocks()

        self.crc32_all()

    def crc32_all(self):
        infomsg(1,'%-10s %-10s %-7s %-10s %s\n'%('start_adr','offset','size','crc32','name'))

        with open(self.filepath,'rb') as fh:
            for b in self.blocks:
                if b['size']:
                    fh.seek(b['offset'])
                    data=fh.read(b['size'])
                    b['crc32'] = zlib.crc32(data)
                else:
                    b['crc32'] = 0
                infomsg(1,'0x%08x 0x%08x %7d 0x%08x %s\n'%(b['start_adr'],b['offset'],b['size'],b['crc32'],b['name']))

        infomsg(1,"%d blocks %d bytes %d%%\n"%(len(self.blocks),self.total_size,100*self.total_size/self.filesize))


def process_dump(sub, dump_name,stubs_data, custom_blocks=None):
    if not os.path.isfile(dump_name):
        warn("missing %s"%(dump_name))
        return

    csums = ChecksumInfo(dump_name)
    if csums.filesize == 0:
        warn("zero size %s"%(dump_name))
        return

    if custom_blocks:
        csums.process_custom(stubs_data, custom_blocks)
    else:
        csums.process_stubs(stubs_data)

    canon_sub = (sub[0]+'.'+sub[1:]).upper()

    # most cams version starts with GM, a few early ones don't
    # assumes all firmware of a given model either do or don't
    if stubs_data.stubs_entry_misc['fw_ver_info']['verstr'][0:2] == 'GM':
        canon_sub = 'GM' + canon_sub

    # sanity check that our inferred firmware version appears in the dump at the expected place
    ver_offset = stubs_data.stubs_entry_misc['fw_ver_info']['verstr_adr'] - stubs_data.stubs_entry_misc['ar_rom']['start_adr']
    if ver_offset + len(canon_sub) <= csums.filesize:
        with open(dump_name,'rb') as fh:
            fh.seek(ver_offset)
            dump_sub=fh.read(len(canon_sub))
            if bytes(canon_sub,'utf8') != dump_sub:
                warn("Version string %s does not match dump %s"%(canon_sub,dump_sub))
                return

    else:
        warn("Version string outside dump, not verified")

    return {
        'sub':sub,
        'canon_sub':canon_sub,
        'csums':csums,
    }

# expects csv of 'start,end' and optionally name
def load_custom_blocks(csvfile):
    r = []
    with open(csvfile) as fh:
        i = 0
        for row in csv.DictReader(fh):
            start_adr = row.get('start')
            if not start_adr:
                sys.stderr.write("ERROR: expected column 'start'\n")
                sys.exit(1)

            end_adr = row.get('end')
            if not end_adr:
                sys.stderr.write("ERROR: expected column 'end'\n")
                sys.exit(1)

            start_adr = int(start_adr,0)
            end_adr = int(end_adr,0)

            if end_adr <= start_adr:
                sys.stderr.write("ERROR: end <= start\n")
                sys.exit(1)

            r.append({
                'start_adr':start_adr,
                'end_adr':end_adr,
                'name':row.get('name',''),
            })

    return r


def fwsums_main():
    argparser = argparse.ArgumentParser(description='''\
Generate crc32 checks of Canon firmware for the firmware_crc module (fwcrc.flt).
Output is a list of address, size, crc lines in hex.
Module is available from Miscellaneous Stuff -> Tools -> Checksum Canon firmware in the CHDK menu.
''',
    formatter_class=argparse.RawDescriptionHelpFormatter)
    argparser.add_argument('platform', help='CHDK platform name, like ixus130_elph140')
    argparser.add_argument('sub', help='Firmware sub name, like 100a')
    argparser.add_argument("-s", "--source",
        help="CHDK source root, default current tree", action='store',
        default=os.path.abspath(os.path.join(self_dir,'..')))
    argparser.add_argument("-d", "--dumps", help="CHDK dumps root, default PRIMARY_ROOT or source platform dir",action='store')
    argparser.add_argument("-o", "--out", help="Output file, default stdout",action='store')
    argparser.add_argument('-v','--verbose',help="Increase verbosity", action='count',default=0)
    argparser.add_argument("--stubsub", help="Firmware stubs, if copied sub",action='store')
    argparser.add_argument("--stubplat", help="Platform for stubs, if copied platform",action='store')
    argparser.add_argument("--dumpfile", help="specify dump file directly instead of inferring from platform/sub",action='store')
    argparser.add_argument("--camlist", help="camera_list.csv instead of inferring from current tree",action='store')
    argparser.add_argument("--header", help="output as header file",action='store_true', default=False)
    argparser.add_argument("--custom-blocks", help="CSV defining custom ranges to CRC",action='store')

    args = argparser.parse_args()

    chdklib.logutil.verbosity = args.verbose

    stubs_data = StubsData(warnfunc=warn) # only warnings, no info

    plat = args.platform
    platsub = args.sub
    chdk_root = args.source
    if args.dumps:
        dump_root = args.dumps
    else:
        dump_root = os.environ.get('PRIMARY_ROOT',os.path.join(chdk_root,'platform'))

    if args.stubsub:
        stubsub = args.stubsub
    else:
        stubsub = platsub

    if args.stubplat:
        stubplat = args.stubplat
    else:
        stubplat = plat

    plat_dir = os.path.join(chdk_root,'platform',stubplat,'sub',stubsub)
    if args.dumpfile:
        if args.header:
            sys.stderr.write("ERROR: --dumpfile cannot be combined with --header\n")
            sys.exit(1)

        dump_name = args.dumpfile
    else:
        dump_name = os.path.join(dump_root,plat,'sub',platsub,'PRIMARY.BIN')

    if args.camlist:
        camlist = args.camlist
    else:
        camlist = os.path.join(chdk_root,'camera_list.csv')

    custom_blocks = None
    if args.custom_blocks:
        # would need to be able to specify per-sub
        if args.header:
            sys.stderr.write("ERROR: --custom-blocks cannot be combined with --header\n")
            sys.exit(1)

        custom_blocks = load_custom_blocks(args.custom_blocks)

    stubs_entry_name = os.path.join(plat_dir,'stubs_entry.S')

    infomsg(1,'Loading stubs %s %s %s\n'%(stubplat,stubsub,stubs_entry_name))

    if not os.path.isfile(stubs_entry_name):
        sys.stderr.write("ERROR: missing %s\n"%(stubs_entry_name))
        sys.exit(1)

    stubs_data.load_stubs_s(stubs_entry_name,process_comments=True)
    stubs_data.guess_platform_vals()
    smisc = stubs_data.stubs_entry_misc

    if 'fw_ver_info' not in smisc:
        sys.stderr.write("ERROR: firmware version string not found in %s\n"%(stubs_entry_name))
        sys.exit(1)

    r = []
    if args.header:
        subs = [stubsub]
        with open(camlist) as fh:
            for row in csv.DictReader(fh):
                if row['CAMERA'] == plat and row['SOURCE_FIRMWARE'] == platsub and row['FIRMWARE'] != platsub:
                    subs.append(row['FIRMWARE'])

        for s in subs:
            dump_name = os.path.join(dump_root,plat,'sub',s,'PRIMARY.BIN')

            infomsg(1,'%s %s %s\n'%(plat,s,dump_name))
            result = process_dump(s,dump_name,stubs_data)

            if result:
                r.append(result)

    else:
        infomsg(1,'%s %s %s\n'%(plat,platsub,dump_name))
        result = process_dump(platsub,dump_name,stubs_data,custom_blocks)
        if result:
            r.append(result)

    if not len(r):
        sys.stderr.write("ERROR: no valid subs processed, exiting\n")
        sys.exit(1)

    if args.out:
        outfile = open(args.out,'w', newline='\n')
    else:
        outfile = sys.stdout

    if args.header:
        outfile.write('/* THIS FILE IS GENERATED, DO NOT EDIT! */\n\n')
        for s in r:
            outfile.write('const firmware_crc_block_t firmware_%s_crc32[]={\n'%(s['sub']))
            for b in s['csums'].blocks:
                outfile.write('    { (const char *)%#10x, %#10x, %#10x }, // %s\n'%(b['start_adr'],b['size'],b['crc32'],b['name']))
            outfile.write('};\n\n');

        outfile.write('firmware_crc_sub_t firmware_crc_list[]={\n')
        for s in r:
            outfile.write('    { "%s", firmware_%s_crc32 },\n'%(s['canon_sub'],s['sub']))
        outfile.write('};\n\n');
        # all subs have the same number of blocks, if some dumps are truncated, they will have zero size
        outfile.write('''\
const firmware_crc_desc_t firmware_crc_desc={
    (const char *)%#x, // firmware_ver_ptr
    firmware_crc_list,
    %d,  // sub_count
    %d,  // block_count
};
'''%(smisc['fw_ver_info']['verstr_adr'],len(r),len(r[0]['csums'].blocks)))


    else:
        for b in r[0]['csums'].blocks:
            if b['size']:
                outfile.write('%x %x %x\n'%(b['start_adr'],b['size'],b['crc32']))

    if args.out:
        outfile.close()


fwsums_main()
