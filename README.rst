PyBCY
=====

This is a Python implementation for the cosplay website bcy.net. Tested
on Python2. Should work on Python3 as well

Daemon
------

A command line tool *PyBCYDownloader.py* will be installed and made
available on *$PATH*, it serves as a simple HTTPServer that accept POST
requests, extract and download corresponding BCY.net posts, it take
three arguments, example:

::

     --Email YOUREMAIL@example.com --Password=YOURPASSWORD  --SavePath=YOURDOWNLOADFOLDERPATH

or you can pass ``Daemon=True`` when creating BCYDownloadUtils to get a
Daemon process.Note that in the latter case you still have to keep your
process running by yourself

Chrome Extension
----------------

Companion Chrome Extension is packed within the distribution, underlying
powered by *PyBCYDownloader.py* as daemon.

Default copied to ``UserHomeDirectory/PyBCYChromeExtension/``

Refer to https://developer.chrome.com/extensions/getstarted#unpacked for
following steps

CHANGELOGS
----------

1.2.4
~~~~~

Fixed a legacy issue that WorkInfo is saved in Base64 encoded format. To
update existing database, run this python script (Tested on Python2):

.. code:: python

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

1.2.6
-----

Force *DownloadWorker* to use ``/PATH/TO/DOWNLOAD/FOLDER/DownloadTemp/``
as temporary file for less I/O cost ## 1.2.8 Use LIFO Queue for better
overall performance with HTTPServer On

1.3.2
-----

| Rename SQL Database Columns For Better Extensibility and overall
  performance.
| Migrate Script From
  http://stackoverflow.com/questions/75675/how-do-i-dump-the-data-of-some-sqlite3-tables.

::

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

Then rename *BCYInfoNew.db* to *BCYInfo.db*, remove *BCYUserNameUID.db*.
Backup old ones if needed

1.3.5
-----

Added Seperate Columns For Tags, which contains JSON-Serialized Version
of Tag-List

::

      ALTER TABLE WorkInfo ADD COLUMN Tags STRING;

And run python script:

.. code:: python

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

1.4.0
-----

| Renamed detail method signatures in **BCYCore**. Results in:
| **+** Much Faster(And More Stable) Querying

This should have no effect on you unless you are not using the detail
wrapper
