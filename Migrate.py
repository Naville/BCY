import sqlite3
import json
import re
oldDB=None
newDB=None
mapping=dict()
if __name__ == '__main__':
    oldDB=sqlite3.connect("/Users/Naville/BCY/BCYInfo.db")
    newDB=sqlite3.connect("/Users/Naville/BCY/BCYInfoNEW.db")
    newDB.execute("CREATE TABLE IF NOT EXISTS UserInfo (uid INTEGER,uname STRING,self_intro STRING,avatar STRING,isValueUser INTEGER,Tags STRING NOT NULL DEFAULT '[]',UNIQUE(uid) ON CONFLICT IGNORE)")
    newDB.execute("CREATE TABLE IF NOT EXISTS ItemInfo (uid INTEGER DEFAULT 0,item_id INTEGER DEFAULT 0,Title STRING NOT NULL DEFAULT '',Tags STRING NOT NULL DEFAULT '[]',ctime INTEGER DEFAULT 0,Description STRING NOT NULL DEFAULT '',Images STRING NOT NULL DEFAULT '[]',VideoID STRING NOT NULL DEFAULT '',UNIQUE (item_id) ON CONFLICT REPLACE)")
    print("Constructing UID Mapping")
    x=oldDB.execute("SELECT uid,Info FROM WorkInfo").fetchall()
    for item in x:
        uid=item[0]
        kv=json.loads(item[1])
        UserName=kv["profile"]["uname"]
        intro=kv["profile"]["self_intro"]
        if intro==None:
            intro=""
        avatar=kv["profile"]["avatar"]
        if avatar.startswith("//"):
            # Really? ByteDance?
            avatar="https:"+avatar
        isV=0
        if kv["profile"]["value_user"]==True:
            isV=1
        mapping[uid]=(UserName,intro,avatar,isV)
    print("Writing mapping...")
    for k in mapping.keys():
        UserName,intro,avatar,isV=mapping[k]
        newDB.execute("INSERT INTO UserInfo(uid,uname,self_intro,avatar,isValueUser) VALUES(?,?,?,?,?)",(k,UserName,intro,avatar,isV,))
    print("Mapping Info")
    x=oldDB.execute("SELECT uid,item_id,Info,Tags,Title FROM WorkInfo").fetchall()
    for item in x:
        uid=item[0]
        item_id=item[1]
        Info=json.loads(item[2])
        Info.pop("properties",None)
        Info.pop("profile",None)
        Info.pop("item_id",None)
        Info.pop("uid",None)
        Info.pop("like_count",None)
        Info.pop("recommend_rela",None)
        Info.pop("post_tags",None)
        plain=Info.pop("plain","")
        Multi=Info.pop("multi",[])
        VideoID=Info.pop("video_info",{}).pop("vid","")
        x=list()
        for foo in Multi:
            foo.pop("type","")
            x.append(foo)
        if "cover" in Info.keys():
            xxx=Info["cover"]
            if len(xxx)>0:
                x.append(xxx)
        Multi=x
        #Handle larticle
        content=Info.pop("content","")
        if content!="":
            x=re.findall("<img src=\"(.{80,100})\" alt=",content)
            for URL in x:
                Multi.append({"path":URL})
        Multi=json.dumps(Multi)
        ctime=Info.pop("ctime",0)
        #Info=json.dumps(Info,ensure_ascii=False,sort_keys=True,separators=(',', ':'))
        Tags=item[3]
        Title=item[4]
        newDB.execute("INSERT INTO ItemInfo(uid,item_id,Tags,ctime,Description,Images,Title,VideoID) VALUES(?,?,?,?,?,?,?,?)",(uid,item_id,Tags,ctime,plain,Multi,Title,VideoID,))
    mapping.clear()
    oldDB.commit()
    oldDB.close()
    newDB.commit()
    newDB.close()
    print("TODO: Manually merge remaining tables")
