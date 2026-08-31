/* THIS FILE IS GENERATED, DO NOT EDIT! */

const firmware_crc_block_t firmware_102c_crc32[]={
    { (const char *)0xffc00000,   0x252156, 0x4a507a12 }, // ROMCODE
    { (const char *)0xffeabed8,     0xd2f8, 0x37e8b7b1 }, // RAM data
};

firmware_crc_sub_t firmware_crc_list[]={
    { "GM1.02C", firmware_102c_crc32 },
};

const firmware_crc_desc_t firmware_crc_desc={
    (const char *)0xffc56545, // firmware_ver_ptr
    firmware_crc_list,
    1,  // sub_count
    2,  // block_count
};
