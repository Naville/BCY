# PyBCY
This is a Python implementation for the cosplay website bcy.net. Tested on Python2. Should work on Python3 as well

## CHANGELOGS

### 1.2.4
  Fixed a legacy issue that WorkInfo is saved in Base64 encoded format. To update existing database, run this python script (Tested on Python2):

```python
import sqlite3,os,sys,json,base64
savepath="/PATH/TO/DOWNLOAD/FOLDER/"
InfoSQL=sqlite3.connect(os.path.join(savepath,"BCYInfo.db"))
Cursor=InfoSQL.execute("SELECT Info from WorkInfo").fetchall()
i=1
for item in Cursor:
    try:
        print ("Re-encoding "+str(i)+"/"+str(len(Cursor)))
        i=i+1
        Info=item[0]
        values=list()
        DecodedData=base64.b64decode(Info)
        DecodedInfo=json.loads(DecodedData)
        values.append(json.dumps(DecodedInfo))
        values.append(Info)
        InfoSQL.execute("UPDATE WorkInfo SET Info=? WHERE Info=?",values)
    except:
        pass

InfoSQL.commit()

```

## 1.2.6
  Force *DownloadWorker* to use ``` /PATH/TO/DOWNLOAD/FOLDER/DownloadTemp/ ``` as temporary file for less I/O cost
## 1.2.8
  Use LIFO Queue for better overall performance with HTTPServer On

## 1.3.2
  Rename SQL Database Columns For Better Extensibility and overall performance.  
  Migrate Script From <http://stackoverflow.com/questions/75675/how-do-i-dump-the-data-of-some-sqlite3-tables>.

```
  sqlite3 BCYInfo.db ".dump 'WorkInfo'" |grep -v 'CREATE TABLE' > BCYInfodump.sql
  echo "CREATE TABLE IF NOT EXISTS WorkInfo (uid STRING NOT NULL DEFAULT '',Title STRING NOT NULL DEFAULT '',cp_id STRING NOT NULL DEFAULT '',rp_id STRING NOT NULL DEFAULT '',dp_id STRING NOT NULL DEFAULT '',ud_id STRING NOT NULL DEFAULT '',post_id STRING NOT NULL DEFAULT '',Info STRING NOT NULL DEFAULT '',UNIQUE(UID,cp_id,rp_id,dp_id,ud_id,post_id) ON CONFLICT REPLACE);CREATE TABLE IF NOT EXISTS UserInfo (uid STRING,UserName STRING);CREATE TABLE IF NOT EXISTS GroupInfo (gid STRING,GroupName STRING);" > tmp.sql
  sqlite3 BCYInfoNew.db < tmp.sql

  sed -i.bak  's/NULL/'"'0'"'/g' ./BCYInfodump.sql
  sqlite3 BCYInfoNew.db < BCYInfodump.sql
  sqlite3 BCYUserNameUID.db ".dump 'GroupInfo'" |grep -v 'CREATE TABLE' > GInfo.sql
  sqlite3 BCYUserNameUID.db ".dump 'UserInfo'" |grep -v 'CREATE TABLE' > UInfo.sql
  sed -i.bak  's/GID/gid/g' ./GInfo.sql
  sed -i.bak  's/UID/uid/g' ./UInfo.sql
  sqlite3 BCYInfoNew.db < GInfo.sql
  sqlite3 BCYInfoNew.db < UInfo.sql

  rm BCYInfodump.sql
  rm tmp.sql
```
  Then rename *BCYInfoNew.db* to *BCYInfo.db*, remove *BCYUserNameUID.db*. Backup old ones if needed

## 1.3.5
  Added Seperate Columns For Tags, which contains JSON-Serialized Version of Tag-List

```
  ALTER TABLE WorkInfo ADD COLUMN Tags STRING;
```
  And run python script:
```python
  import sqlite3,os,sys,json
  savepath="/PATH/TO/DOWNLOAD/ROOT/"
  InfoSQL=sqlite3.connect(os.path.join(savepath,"BCYInfo.db"))
  Cursor=InfoSQL.execute("SELECT Info from WorkInfo").fetchall()
  i=1
  for item in Cursor:
      Info=None
      try:
          print ("Re-encoding "+str(i)+"/"+str(len(Cursor)))
          i=i+1
          values=list()
          TagList=list()
          raw=item[0]
          Info=json.loads(raw)
          for item in Info.get("post_tags",list()):
              TagList.append(item["tag_name"])
          values.append(json.dumps(TagList,separators=(',', ':'),ensure_ascii=False, encoding='utf8'))
          values.append(raw)
          InfoSQL.execute("UPDATE WorkInfo SET Tags=? WHERE Info=?",values)
      except:
          InfoSQL.commit()
          print Info
          raise
```
## 1.4.0
  Renamed detail method signatures in **BCYCore**.
  Results in:  
  **+** Much Faster(And More Stable) Querying

  This should have no effect on you unless you are not using the detail wrapper

## 1.4.4
  `since` and `to` keywords has been added to `BCYDownloadUtils` 's iterating methods using `**kwargs`. This would speed up list iterations

## 1.6.1
  Python3 compatible

## 1.7.0
  Add Like/Unlike Work. Report Work

## 1.7.1
  Fix a issue in EncryptParam() results in failed Non-ASCII string encoding

## 1.7.5
  Fix a issue in `BCYDownloadUtils` where GroupID is used to construct title instead of post id.


```python
import sqlite3,os,sys,json,base64,os
savepath="/PATH/TO/FOLDER"
InfoSQL=sqlite3.connect(os.path.join(savepath,"BCYInfo.db"))
GroupNameList=list()
GroupsCursor=InfoSQL.execute("SELECT GroupName from GroupInfo").fetchall()
for item in GroupNameList:
    print ("Found GroupName:"+item[0])
    GroupNameList.append(item[0])
Cursor=InfoSQL.execute("SELECT uid,Title,Info from WorkInfo").fetchall()
for item in Cursor:
    try:
        print ("Re-encoding "+str(i)+"/"+str(len(Cursor)))
        UserName=InfoSQL.execute("SELECT UserName FROM UserInfo WHERE uid=?",(item[0],)).next()[0]
        Title=item[1]
        DecodedInfo=json.loads(item[2])
        for GName in GroupNameList:
            if Title.beginsWith(GName+"-") and ("post_id" in DecodedInfo.keys()):
                newTitle=GName+"-"+DecodedInfo["post_id"]
                InfoSQL.execute("UPDATE WorkInfo SET Title=? Title=?",(newTitle,Title,))
                print("Replaced "+UserName+"'s GroupWorkTitle:"+newTitle)
                #Move Folders
                PathRoot=os.path.join(savepath,UserName)
                OldPath=os.path.join(PathRoot,Title)
                newPath=os.path.join(PathRoot,newTitle)
                os.rename(OldPath,newPath)
    except:
        raise

InfoSQL.commit()

```

## 1.9.0
  Add unique constraints to all tables. Better iterator status preservation

```bash
sqlite3 /PATH/TO/NEW/TABLE/BCYInfo.db
CREATE TABLE IF NOT EXISTS UserInfo (uid STRING,UserName STRING,UNIQUE(uid) ON CONFLICT IGNORE);
CREATE TABLE IF NOT EXISTS GroupInfo (gid STRING,GroupName STRING,UNIQUE(gid) ON CONFLICT IGNORE);
CREATE TABLE IF NOT EXISTS WorkInfo (uid STRING NOT NULL DEFAULT '',Title STRING NOT NULL DEFAULT '',cp_id STRING NOT NULL DEFAULT '',rp_id STRING NOT NULL DEFAULT '',dp_id STRING NOT NULL DEFAULT '',ud_id STRING NOT NULL DEFAULT '',post_id STRING NOT NULL DEFAULT '',Info STRING NOT NULL DEFAULT '',Tags STRING,UNIQUE(UID,cp_id,rp_id,dp_id,ud_id,post_id) ON CONFLICT REPLACE);
```

Then apply dump of old table

```bash
sqlite3 /PATH/TO/OLD/TABLE/BCYInfo.db .dump >DUMP.SQL
sqlite3 /PATH/TO/NEW/TABLE/BCYInfo.db < DUMP.SQL
```

## 2.4.0
  Use INTEGER as the data type of WorkInfo's related columns.Below is a python3 mitigation script

```python
import sqlite3
import json
InfoSQL=sqlite3.connect("OLD/DATABASE/")
NewSQL=sqlite3.connect("NEW/DATABASE/")
InfoSQL.text_factory = str
NewSQL.text_factory = str
NewSQL.execute("CREATE TABLE IF NOT EXISTS UserInfo (uid STRING,UserName STRING,UNIQUE(uid) ON CONFLICT IGNORE);")
NewSQL.execute("CREATE TABLE IF NOT EXISTS GroupInfo (gid STRING,GroupName STRING,UNIQUE(gid) ON CONFLICT IGNORE);")
NewSQL.execute("CREATE TABLE IF NOT EXISTS WorkInfo (uid INTEGER DEFAULT 0,Title STRING NOT NULL DEFAULT '',cp_id INTEGER DEFAULT 0,rp_id INTEGER DEFAULT 0,dp_id INTEGER DEFAULT 0,ud_id INTEGER DEFAULT 0,post_id INTEGER DEFAULT 0,Info STRING NOT NULL DEFAULT '',Tags STRING,UNIQUE(uid,cp_id,rp_id,dp_id,ud_id,post_id) ON CONFLICT REPLACE);")
Cursor=InfoSQL.execute("SELECT Title,Info FROM WorkInfo").fetchall()
index=1
for item in Cursor:
    Title=item[0]
    Info=json.loads(item[1])
    tags=list()
    args=list()
    args.append(int(Info["uid"]))
    args.append(Title)
    args.append(int(Info.get("cp_id",0)))
    args.append(int(Info.get("rp_id",0)))
    args.append(int(Info.get("dp_id",0)))
    args.append(int(Info.get("ud_id",0)))
    args.append(int(Info.get("post_id",0)))
    args.append(item[1])
    for foo in Info.get("post_tags",list()):
        tags.append(foo["tag_name"])
    args.append(json.dumps(tags))
    NewSQL.execute("INSERT OR REPLACE INTO WorkInfo(uid,Title,cp_id,rp_id,dp_id,ud_id,post_id,Info,Tags) VALUES(?,?,?,?,?,?,?,?,?)",tuple(args))
    print("WorkInfo Update:%i/%i"%(index,len(Cursor)))
    index=index+1

index=1
Cursor=InfoSQL.execute("SELECT gid,GroupName FROM GroupInfo").fetchall()
for item in Cursor:
    NewSQL.execute("INSERT OR REPLACE INTO GroupInfo(gid,GroupName) VALUES(?,?)",(int(item[0]),item[1]))
    print("GroupInfo Update:%i/%i"%(index,len(Cursor)))
    index=index+1
index=1
Cursor=InfoSQL.execute("SELECT uid,UserName FROM UserInfo").fetchall()
for item in Cursor:
    NewSQL.execute("INSERT OR REPLACE INTO UserInfo(uid,UserName) VALUES(?,?)",(int(item[0]),item[1]))
    print("UserInfo Update:%i/%i"%(index,len(Cursor)))
    index=index+1
NewSQL.commit()
NewSQL.close()
InfoSQL.close()
```
then replace the old database with the new one

## 2.5.1
Fix a legacy issue results in empty identifiers being saved.
Re-run the mitigation script for 2.4.0
