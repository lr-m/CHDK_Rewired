/* THIS FILE IS GENERATED, DO NOT EDIT! */

const firmware_crc_block_t firmware_101b_crc32[]={
    { (const char *)0xffc00000,   0x251716,  0x66d2cf8 }, // ROMCODE
    { (const char *)0xffeab38c,     0xd2a8, 0xc16a79f7 }, // RAM data
};

const firmware_crc_block_t firmware_101a_crc32[]={
    { (const char *)0xffc00000,   0x251716, 0x1df94c67 }, // ROMCODE
    { (const char *)0xffeab38c,     0xd2a8, 0xc16a79f7 }, // RAM data
};

firmware_crc_sub_t firmware_crc_list[]={
    { "GM1.01B", firmware_101b_crc32 },
    { "GM1.01A", firmware_101a_crc32 },
};

const firmware_crc_desc_t firmware_crc_desc={
    (const char *)0xffc56029, // firmware_ver_ptr
    firmware_crc_list,
    2,  // sub_count
    2,  // block_count
};
