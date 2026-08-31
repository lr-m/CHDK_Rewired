/* THIS FILE IS GENERATED, DO NOT EDIT! */

const firmware_crc_block_t firmware_100f_crc32[]={
    { (const char *)0xffc00000,   0x299ad0, 0x6ab836a5 }, // ROMCODE, RAM data
};

const firmware_crc_block_t firmware_100e_crc32[]={
    { (const char *)0xffc00000,   0x299ad0, 0xe2020386 }, // ROMCODE, RAM data
};

firmware_crc_sub_t firmware_crc_list[]={
    { "GM1.00F", firmware_100f_crc32 },
    { "GM1.00E", firmware_100e_crc32 },
};

const firmware_crc_desc_t firmware_crc_desc={
    (const char *)0xffc0ad35, // firmware_ver_ptr
    firmware_crc_list,
    2,  // sub_count
    1,  // block_count
};
