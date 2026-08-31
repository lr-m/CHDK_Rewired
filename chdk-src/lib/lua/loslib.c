/*
** $Id: loslib.c,v 1.19.1.3 2008/01/18 16:38:18 roberto Exp $
** Standard Operating System library
** See Copyright Notice in lua.h
*/

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#ifdef HOST_LUA
#include <sys/stat.h>
#include <utime.h>
#endif

#define loslib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


static int os_pushresult (lua_State *L, int i, const char *filename) {
  int en = errno;  /* calls to Lua API may change this value */
  if (i) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    lua_pushfstring(L, "%s: %s", filename, strerror(en));
    lua_pushinteger(L, en);
    return 3;
  }
}


#ifdef HOST_LUA
static int os_execute (lua_State *L) {
  lua_pushinteger(L, system(luaL_optstring(L, 1, NULL)));
  return 1;
}
#endif


static int os_remove (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  return os_pushresult(L, remove(filename) == 0, filename);
}


static int os_rename (lua_State *L) {
  const char *fromname = luaL_checkstring(L, 1);
  const char *toname = luaL_checkstring(L, 2);
  return os_pushresult(L, rename(fromname, toname) == 0, fromname);
}


// TODO
#if 0
static int os_tmpname (lua_State *L) {
  char buff[LUA_TMPNAMBUFSIZE];
  int err;
  lua_tmpnam(buff, err);
  if (err)
    return luaL_error(L, "unable to generate a unique filename");
  lua_pushstring(L, buff);
  return 1;
}
#endif

#ifdef HOST_LUA
static int os_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}
#endif

#if 0
static int os_clock (lua_State *L) {
  lua_pushnumber(L, ((lua_Number)clock())/(lua_Number)CLOCKS_PER_SEC);
  return 1;
}
#endif


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

static void setfield (lua_State *L, const char *key, int value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

static void setboolfield (lua_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}

static int getboolfield (lua_State *L, const char *key) {
  int res;
  lua_getfield(L, -1, key);
  res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}


static int getfield (lua_State *L, const char *key, int d) {
  int res;
  lua_getfield(L, -1, key);
  if (lua_isnumber(L, -1))
    res = (int)lua_tointeger(L, -1);
  else {
    if (d < 0)
      return luaL_error(L, "field " LUA_QS " missing in date table", key);
    res = d;
  }
  lua_pop(L, 1);
  return res;
}


static int os_date (lua_State *L) {
  const char *s = luaL_optstring(L, 1, "%c");
  time_t t = luaL_opt(L, (time_t)luaL_checknumber, 2, time(NULL));
  struct tm *stm;
  if (*s == '!') {  /* UTC? */
  #if 0
  // reyalp - we have no idea about timezones, so just eat the !
  // and use local time
  // TODO some cams may be timezone/dst aware ?
    stm = gmtime(&t);
  #endif
    stm = localtime(&t);
    s++;  /* skip `!' */
  }
  else
    stm = localtime(&t);
  if (stm == NULL)  /* invalid date? */
    lua_pushnil(L);
  else if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setfield(L, "sec", stm->tm_sec);
    setfield(L, "min", stm->tm_min);
    setfield(L, "hour", stm->tm_hour);
    setfield(L, "day", stm->tm_mday);
    setfield(L, "month", stm->tm_mon+1);
    setfield(L, "year", stm->tm_year+1900);
    setfield(L, "wday", stm->tm_wday+1);
    setfield(L, "yday", stm->tm_yday+1);
    setboolfield(L, "isdst", stm->tm_isdst);
  }
  else {
    char cc[3];
    luaL_Buffer b;
    cc[0] = '%'; cc[2] = '\0';
    luaL_buffinit(L, &b);
    for (; *s; s++) {
      if (*s != '%' || *(s + 1) == '\0')  /* no conversion specifier? */
        luaL_addchar(&b, *s);
      else {
        size_t reslen;
        char buff[200];  /* should be big enough for any conversion result */
        cc[1] = *(++s);
        reslen = strftime(buff, sizeof(buff), cc, stm);
        luaL_addlstring(&b, buff, reslen);
      }
    }
    luaL_pushresult(&b);
  }
  return 1;
}


static int os_time (lua_State *L) {
  time_t t;
  if (lua_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -1);
    ts.tm_mon = getfield(L, "month", -1) - 1;
    ts.tm_year = getfield(L, "year", -1) - 1900;
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
  }
  if (t == (time_t)(-1))
    lua_pushnil(L);
  else
    lua_pushnumber(L, (lua_Number)t);
  return 1;
}


#if 0
static int os_difftime (lua_State *L) {
  lua_pushnumber(L, difftime((time_t)(luaL_checknumber(L, 1)),
                             (time_t)(luaL_optnumber(L, 2, 0))));
  return 1;
}
#endif
static int os_difftime (lua_State *L) {
  lua_pushnumber(L, (time_t)(luaL_checknumber(L, 1) - (time_t)(luaL_optnumber(L, 2, 0))));
  return 1;
}

/* }====================================================== */


#if 0
static int os_setlocale (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = luaL_optstring(L, 1, NULL);
  int op = luaL_checkoption(L, 2, "all", catnames);
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}
#endif

#ifdef HOST_LUA
static int os_exit (lua_State *L) {
  exit(luaL_optint(L, 1, EXIT_SUCCESS));
}
#endif

// reyalp added
static int os_mkdir (lua_State *L) {
  const char *dirname = luaL_checkstring(L, 1);
#if defined(HOST_LUA) && !defined(_WIN32)
  return os_pushresult(L, mkdir(dirname,0777) == 0, dirname);
#else
  return os_pushresult(L, mkdir(dirname) == 0, dirname);
#endif
}

static int get_table_optbool(lua_State *L, int narg, const char *fname, int d)
{
    int r;
	lua_getfield(L, narg, fname);
    // not set - use default
	if(lua_isnil(L,-1)) {
		r=d;
	} else {// otherwise, treat as bool
        r=lua_toboolean(L,-1); 
    }
	lua_pop(L,1);
    return r;
}

/*
  syntax
    t=os.listdir("name"[,opts])
  returns array of filenames, or nil, strerror, errno

  opts may be a boolean or a table
  if opts is a table, it can contain boolean fields:
   "showall" - if true, includes ".", ".." and (depending on OS) deleted entries
   "chdklfn" - if true, returns long file names where possible
  if opts is a boolean, it is equivalent to "showall" above

  NOTE except for the root directory, names ending in / will not work
*/
static int os_listdir (lua_State *L) {
#ifdef HOST_LUA
  DIR *dir;
  struct dirent *de;
  const char *dirname = luaL_checkstring(L, 1);
  int all;
  if(lua_istable(L,2)) {
    all=get_table_optbool(L,2,"showall",0);
    // hostlua does not have lfn stuff, ignore
    // od_flags=(get_table_optbool(L,2,"chdklfn",1))?OPENDIR_FL_CHDK_LFN:OPENDIR_FL_NONE;
  } else {
    all=lua_toboolean(L, 2);
  }
  int i=1;
  dir = opendir(dirname);
  if(!dir) 
    return os_pushresult(L, 0 , dirname);
  lua_newtable(L); 
  while((de = readdir(dir))) {
    if(!all && (de->d_name[0] == '\xE5' || (strcmp(de->d_name,".") == 0) || (strcmp(de->d_name,"..") == 0)))
    continue;
    lua_pushinteger(L, i);
    lua_pushstring(L, de->d_name);
    lua_settable(L,-3);
    ++i;
  }
  closedir(dir);
  return 1;
#else
  DIR *dir;
  struct dirent *de;
  const char *dirname = luaL_checkstring(L, 1);
  int all=0,od_flags=OPENDIR_FL_CHDK_LFN;
  if(lua_istable(L,2)) {
    all=get_table_optbool(L,2,"showall",0);
    od_flags=(get_table_optbool(L,2,"chdklfn",1))?OPENDIR_FL_CHDK_LFN:OPENDIR_FL_NONE;
  } else {
    all=lua_toboolean(L, 2);
  }
  int i=1;
  dir = opendir_chdk(dirname,od_flags);
  if(!dir) 
    return os_pushresult(L, 0 , dirname);
  lua_newtable(L); 
  while((de = readdir(dir))) {
	if(!all && (de->d_name[0] == '\xE5' || (strcmp(de->d_name,".")==0) || (strcmp(de->d_name,"..")==0)))
      continue;
  	lua_pushinteger(L, i);
  	lua_pushstring(L, de->d_name);
	lua_settable(L,-3);
	++i;
  }
  closedir(dir);
  return 1;
#endif
}

#define IDIR_META "chdk_idir_meta"

typedef struct {
    DIR *dir;
    int all;
} idir_udata_t;

static int idir_iter(lua_State *L) {
    struct dirent *de;
    idir_udata_t *ud = (idir_udata_t *)luaL_checkudata(L,1,IDIR_META);
    // dir may be on first call if opendir failed, or previous explicit close
    if(!ud->dir) {
        return 0;
    }
    // allow explicit close by calling iterator(ud,false)
    // need to check type because first invocation is called with nil
    if(lua_type(L, 2) == LUA_TBOOLEAN && lua_toboolean(L,2) == 0) {
        closedir(ud->dir);
        ud->dir=NULL;
        return 0;
    }
    while((de = readdir(ud->dir))) {
        // if not all, skip over ignored items
        if(!ud->all && (de->d_name[0] == '\xE5' || (strcmp(de->d_name,".")==0) || (strcmp(de->d_name,"..")==0))) {
            continue;
        }
        break;
    }
    if(de) {
        lua_pushstring(L, de->d_name);
        return 1;
    } else { // on last, close immediately to avoid keeping the handle open until GC
        closedir(ud->dir);
        ud->dir=NULL;
        return 0;
    }
}

static int idir_gc(lua_State *L) {
    idir_udata_t *ud = (idir_udata_t *)luaL_checkudata(L,1,IDIR_META);
    if(ud->dir) {
        closedir(ud->dir);
    }
    return 0;
}

/*
  syntax
    iteratator, userdata = os.idir("name"[,opts])
  each call to iterator(userdata) returns the next directory entry or nil if all
  entries have been returned
  typical usage
    for fname in os.idir("name"[,opts]) do ...

  opts may be a boolean or a table
  if opts is a table, it can contain boolean fields:
   "showall" - if true, includes ".", ".." and (depending on OS) deleted entries
   "chdklfn" - if true, returns long file names where possible
  if opts is a boolean, it is equivalent to "showall" above

 NOTES:
  Except for the root directory, names ending in / will not work
  If there is an error opening the directory, the results will be identical to
  an empty directory, unless all is true
  The directory handle is kept open until all directory entries are iterated
  or the userdata is GC'd. This means that:
  1) deep recursive traversals may run into handle limits. 
  2) If you break out of a loop using the iterator, the handle may remain open for a while.
  Explicitly calling iterator(userdata,false) will immediately close the handle. This cannot
  be used with the typical for syntax given above, you must use an explicit while loop like:
  local idir,ud=os.idir('A/')
  repeat
    name=idir(ud)
    -- break out of loop under some condition
    if somefunction(name) then
        break
    end
  until not name
  idir(ud,false) -- ensure directory handle is closed
*/
static int os_idir (lua_State *L) {
#ifdef HOST_LUA
  const char *dirname = luaL_checkstring(L, 1);
  int all;
  if(lua_istable(L,2)) {
    all=get_table_optbool(L,2,"showall",0);
    // hostlua does not have lfn stuff, ignore
    // od_flags=(get_table_optbool(L,2,"chdklfn",1))?OPENDIR_FL_CHDK_LFN:OPENDIR_FL_NONE;
  } else {
    all=lua_toboolean(L, 2);
  }

  lua_pushcfunction(L, idir_iter);

  idir_udata_t *ud = lua_newuserdata(L,sizeof(idir_udata_t));
  ud->dir = opendir(dirname); // may be null, in which case iterator will stop on first iteration
                              // no obvious way to return error status
  ud->all = all;

  luaL_getmetatable(L, IDIR_META);
  lua_setmetatable(L, -2);
  return 2;
#else
  const char *dirname = luaL_checkstring(L, 1);
  int all=0,od_flags=OPENDIR_FL_CHDK_LFN;
  if(lua_istable(L,2)) {
    all=get_table_optbool(L,2,"showall",0);
    od_flags=(get_table_optbool(L,2,"chdklfn",1))?OPENDIR_FL_CHDK_LFN:OPENDIR_FL_NONE;
  } else {
    all=lua_toboolean(L, 2);
  }

  lua_pushcfunction(L, idir_iter);

  idir_udata_t *ud = lua_newuserdata(L,sizeof(idir_udata_t));
  ud->dir = opendir_chdk(dirname,od_flags); // may be null, in which case iterator will stop on first iteration
                                // no obvious way to return error status
  ud->all = all;

  luaL_getmetatable(L, IDIR_META);
  lua_setmetatable(L, -2);
  return 2;
#endif
}

static const luaL_Reg idir_meta_methods[] = {
  {"__gc", idir_gc},
  {NULL, NULL}
};
static void idir_register(lua_State *L) {
    luaL_newmetatable(L,IDIR_META);
    luaL_register(L, NULL, idir_meta_methods);  
}

// t = stat("name")
// nil,strerror,errno on fail
static int os_stat (lua_State *L) {
  struct stat st;
  const char *name = luaL_checkstring(L, 1);
  int result = stat(name,&st);
  if (result==0) {
    lua_createtable(L, 0, 6);  /* = number of fields */
    // don't expose the fields that aren't useful
	// but leave them commented out for reference
//#ifndef CAM_DRYOS_2_3_R39
//    setfield(L,"dev",st.st_dev);		/* device ID number */
//    setfield(L,"ino",st.st_ino);		/* no inodes in fat, always -1 */
//    setfield(L,"mode",st.st_mode);	/* file mode (see below) */
//#endif
//    setfield(L,"nlink",st.st_nlink);	/* dryos 0, vxworks 1 */
//    setfield(L,"uid",st.st_uid);		/* no users or groups on fat */
//    setfield(L,"gid",st.st_gid);		/* " */
//#if !CAM_DRYOS
// doesn't appear useful, I wasn't able to stat any special files
//    setfield(L,"rdev",st.st_rdev);	/* device ID, only if special file */
//#endif
    setfield(L,"size",st.st_size);	/* size of file, in bytes */
//#ifndef CAM_DRYOS_2_3_R39
//    setfield(L,"atime",st.st_atime);	/* time of last access */
//#endif
    setfield(L,"mtime",st.st_mtime);	/* time of last modification */
    setfield(L,"ctime",st.st_ctime);	/* time of last change of file status */
#ifdef HOST_LUA
// fill in some sane values if we aren't running on the camera
// from chdk stdlib
#define DOS_ATTR_DIRECTORY      0x10            /* entry is a sub-directory */
#ifndef CAM_DRYOS_2_3_R39
    setfield(L,"blksize",512); 
    setfield(L,"blocks",(st.st_size/512) + (st.st_size%512)?1:0); 
#endif
    if ( S_ISDIR(st.st_mode) ) {
      setfield(L,"attrib",DOS_ATTR_DIRECTORY);
      setboolfield(L,"is_dir",1);
      setboolfield(L,"is_file",0);
    }
    else {
      setboolfield(L,"is_dir",0);
      setfield(L,"attrib",0);
      if S_ISREG(st.st_mode) {
        setboolfield(L,"is_file",1);
      }
    }
#else
//#ifndef CAM_DRYOS_2_3_R39
//    setfield(L,"blksize",st.st_blksize); /* This is NOT the dos sector size. Appears to be 512 on all I've tested! */
//    setfield(L,"blocks",st.st_blocks);   /* number of blocks required to store file. May not be the same as size on disk, per above*/
//#endif
    setfield(L,"attrib",st.st_attrib);	/* file attribute byte (dosFs only) */
	// for convenience
	// note volume labels are neither file nor directory
	setboolfield(L,"is_dir",st.st_attrib & DOS_ATTR_DIRECTORY);
	setboolfield(L,"is_file",!(st.st_attrib & (DOS_ATTR_DIRECTORY | DOS_ATTR_VOL_LABEL)));
#endif
#if 0
    setfield(L,"reserved1",st.reserved1);
    setfield(L,"reserved2",st.reserved2);
    setfield(L,"reserved3",st.reserved3);
    setfield(L,"reserved4",st.reserved4);
    setfield(L,"reserved5",st.reserved5);
    setfield(L,"reserved6",st.reserved6);
#endif
    return 1;
  }
  else {
    int en = errno;
    lua_pushnil(L);
    lua_pushfstring(L, "%s: %s", name, strerror(en));
    lua_pushinteger(L, en);
    return 3;
  }
}

// utime(name,[modtime,[actime]])
// true | nil,strerror, errno
// current time used for missing or nil args
static int os_utime (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  struct utimbuf t;
  t.modtime = luaL_optnumber(L, 2, time(NULL));
  t.actime = luaL_optnumber(L, 3, time(NULL));
  return os_pushresult(L, utime(name,&t) == 0, name);
}

static const luaL_Reg syslib[] = {
#if 0
  {"clock",     os_clock},
#endif
  {"date",      os_date},
  {"difftime",  os_difftime},
#ifdef HOST_LUA
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
#endif
  {"mkdir",     os_mkdir}, // reyalp - NOT STANDARD
  {"listdir",   os_listdir}, // reyalp - NOT STANDARD
  {"idir",      os_idir}, // reyalp - NOT STANDARD
  {"stat",      os_stat}, // reyalp - NOT STANDARD
  {"utime",     os_utime}, // reyalp - NOT STANDARD
  {"remove",    os_remove},
  {"rename",    os_rename},
#if 0
  {"setlocale", os_setlocale},
#endif
  {"time",      os_time},
#if 0
  {"tmpname",   os_tmpname},
#endif
  {NULL, NULL}
};

/* }====================================================== */



LUALIB_API int luaopen_os (lua_State *L) {
  idir_register(L);
  luaL_register(L, LUA_OSLIBNAME, syslib);
  return 1;
}

