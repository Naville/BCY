# -*- coding: utf-8 -*-
import os,errno,tempfile,sqlite3,sys,shutil,threading,time,json,struct,requests,logging,re
from PyBCY.BCYDownloadFilter import *
from PyBCY.BCYCore import *
from sys import version as python_version
from cgi import parse_header, parse_multipart

if python_version.startswith('3'):
    from urllib.parse import parse_qs
    from http.server import BaseHTTPRequestHandler, HTTPServer
else:
    from urlparse import parse_qs
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
try:
    reload(sys)
    sys.setdefaultencoding('utf8')
except:
    pass
try:
    import Queue as Queue
except ImportError:
    try:
        import queue as Queue
    except ImportError:
        raise


class ServerHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(200, "ok")
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header("Access-Control-Allow-Headers", "X-Requested-With")
        self.send_response(200, "ok")
    def do_HEAD(self):
        self._set_headers()
    def do_POST(self):
        Info= self.rfile.read(int(self.headers.getheader('Content-Length')))
        Info=json.loads(Info)
        self.ParseWebURL(Info["URL"])
        self.send_response(200, "ok")
    def ParseWebURL(self,URL):
        Regex=r'(?P<Type>illust|coser|daily)\/detail\/(?P<Component>[0-9\/]*)'
        Regex2=r'bcy.net\/u\/([0-9]*)'
        R1=re.compile(Regex)
        R2=re.compile(Regex2)
        m=R1.search(URL)
        m2=R2.search(URL)

        if m2!=None:
            self.server.DownloadUtils.logger.warning("HTTPDownloadRequest User uid:%s"%(m2.group(0)))
            self.server.DownloadUtils.DownloadUser(m2.group(0),"all")
        if m!=None:
            if m.group("Type")=="daily":
                self.server.DownloadUtils.logger.warning("HTTPDownloading Daily %s"%m.group("Component"))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"ud_id":m.group("Component")})
            elif m.group("Type")=="coser":
                cp_id=m.group("Component").split("/")[0]
                rp_id=m.group("Component").split("/")[1]
                self.server.DownloadUtils.logger.warning("HTTPDownloadRequest Cosplay cp_id:%s rp_id:%s"%(cp_id,rp_id))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"cp_id":cp_id,"rp_id":rp_id})
            elif m.group("Type")=="illust":
                dp_id=m.group("Component").split("/")[0]
                rp_id=m.group("Component").split("/")[1]
                self.server.DownloadUtils.logger.warning("HTTPDownloadRequest Illust dp_id:%s rp_id:%s"%(dp_id,rp_id))
                self.server.DownloadUtils.DownloadFromAbstractInfo({"dp_id":dp_id,"rp_id":rp_id})
            else:
                return
class BCYDownloadUtils(object):
    '''
        self.DownloadProcesses          字典。键为当前正在下载的URL。值为(已下载大小,总大小)的tuple
        self.logger                     一个logging.logger() 对象
        self.Filter                     A BCYDownloadFilter() object used for filtering downloads
        self.API                        A BCYCore() object
        self.FailedInfoList             一个查询详情失败的作品信息列表
        self.Status                     一个用于记录列表迭代进度的字典
        self.InfoSQL                    A sqlite3 connection object

        BCYDownloadUtils is underlying powered by two FIFO Queues:
            QueryQueue          For Storing AbstractInfo
            DownloadQueue       Storing Info used by DownloadWorker(). DO NOT INVOKE DIRECTLY

        BCYDownloadUtils的所有方法实质上都是对两个FIFO队列的操作:
            QueryQueue          用于存储作品信息
            DownloadQueue       保存需要下载的详情。由DownloadWorker()来使用,由DownloadFromInfo()填充。请勿直接写入
        半次元新增IP并发请求数和请求速率限制之后建议每次调用方法之后join()一次避免速度过快被Ban
    '''
    def __init__(self,email,password,savepath,MaxDownloadThread=16,MaxQueryThread=64,Daemon=False,IP="127.0.0.1",Port=8081,DownloadProgress=False,DatabaseFileName="BCYInfo.db",UseCachedDetail=True):
        '''
        传入None的email password可实现匿名登录。 注意此种情况下许多需要登录的API无法使用
        UseCachedDetail为True时查询作品详情时会优先查询SQL内的缓存以避免昂贵的API访问带来的频次限制开销
        '''
        self.UseCachedDetail=UseCachedDetail
        self.QueryQueue=Queue.Queue()
        self.Status=dict()
        self.DownloadQueue=Queue.Queue()
        self.SavePath=savepath
        self.logger=logging.getLogger(str(__name__)+"-"+str(hex(id(self))))
        self.logger.addHandler(logging.NullHandler())
        self.Filter=BCYDownloadFilter()
        self.API=BCYCore()

        self.FailedInfoList=list()
        if email!=None and password!=None:
            self.API.loginWithEmailAndPassWord(email,password)
            print ("Logged in...UID:"+str(self.API.UID))
        else:
            print ("Anonymous Login")
        self.InfoSQL=sqlite3.connect(os.path.join(savepath,DatabaseFileName),check_same_thread=False)
        self.InfoSQL.text_factory = str
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS UserInfo (uid INTEGER,UserName STRING,UNIQUE(uid) ON CONFLICT IGNORE)")
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS GroupInfo (gid INTEGER,GroupName STRING,UNIQUE(gid) ON CONFLICT IGNORE)")
        self.InfoSQL.execute("CREATE TABLE IF NOT EXISTS WorkInfo (uid INTEGER DEFAULT 0,Title STRING NOT NULL DEFAULT '',cp_id INTEGER DEFAULT 0,rp_id INTEGER DEFAULT 0,dp_id INTEGER DEFAULT 0,ud_id INTEGER DEFAULT 0,post_id INTEGER DEFAULT 0,Info STRING NOT NULL DEFAULT '',Tags STRING,UNIQUE(uid,cp_id,rp_id,dp_id,ud_id,post_id) ON CONFLICT REPLACE)")
        self.InfoSQL.execute("PRAGMA journal_mode=WAL;")
        self.InfoSQLLock=threading.Lock()
        self.DownloadProcesses=dict()
        #Create Threading Events
        self.DownloadWorkerEvent=threading.Event()
        self.QueryEvent=threading.Event()
        self.DownloadWorkerEvent.set()
        self.QueryEvent.set()
        #End Threading Events Init

        #Prepare Threads
        for i in range(MaxDownloadThread):
            t =  threading.Thread(target=self.DownloadWorker)
            t.daemon = Daemon
            t.start()
        for i in range(MaxQueryThread):
            t =  threading.Thread(target=self.DownloadFromAbstractInfoWorker)
            t.daemon = Daemon
            t.start()
        #Wakeup
        if Daemon==True:
            server_address = (IP,int(Port))
            httpd = HTTPServer(server_address, ServerHandler)
            httpd.DownloadUtils=self
            t =  threading.Thread(target=httpd.serve_forever)
            t.daemon = Daemon
            t.start()
        #Create Temporary Folder
        try:
            os.makedirs(os.path.join(self.SavePath,"DownloadTemp/"))
        except OSError as exception:
            if exception.errno != errno.EEXIST:
                raise
        self.DownloadProgress=DownloadProgress
    def __enter__(self):
        return self
    def __exit__(self, type, value, trace):
        self.cancel()
    def DownloadWorker(self):
        '''
        供__init__ fork新线程使用。请勿直接调用。
        本方法从DownloadQueue中获取下载任务并实际执行
        '''
        def Logger(URL,Downloaded,total_length):
            if Downloaded==total_length:
                self.DownloadProcesses.pop(URL, None)
            self.DownloadProcesses[URL]=(Downloaded,total_length)

        while self.DownloadWorkerEvent.isSet()==True:
            try:
                obj=self.DownloadQueue.get(block=True)
                URL=obj[0]
                ID=obj[1]
                WorkType=obj[2]
                SaveBase=obj[3]
                FileName=obj[4]
                ctime=obj[5]
                SavePath=os.path.join(SaveBase,FileName)
                try:
                    os.makedirs(SaveBase)
                except OSError as exception:
                    if exception.errno != errno.EEXIST:
                        raise
                if self.DownloadProgress==True:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType},Callback=Logger)
                else:
                    ImageData=self.API.imageDownload({"url":URL,"id":ID,"type":WorkType})
                #Atomic Writing
                if ImageData!=None and len(ImageData) > 16:
                    f=tempfile.NamedTemporaryFile(dir=os.path.join(self.SavePath,"DownloadTemp/"),delete=False, suffix="PyBCY-")
                    f.write(ImageData)
                    f.close()
                    shutil.move(f.name,SavePath)
                    os.utime(savePath,(ctime,ctime))
            except:
                pass
            self.DownloadQueue.task_done()
    def DownloadFromInfo(self,Info,SaveInfo=True):
        '''
        分析作品详情从中:
            拼接路径等信息
            保存详情
            分发下载任务到DownloadQueue
        '''
        if type(Info)!=dict or "uid" not in Info.keys():
            self.logger.error("Found Invalid Info:%s",Info)
            return
        UID=Info["uid"]
        Title=Info.get("title",None)
        CoserName=self.LoadOrSaveUserName(Info["profile"]["uname"],UID)
        WorkID=None
        WorkType=None
        if self.Filter.ShouldBlockInfo(Info)==True:
            return
        if "ud_id" in Info.keys():
            WorkID=Info["ud_id"]
            WorkType="daily"
            if(Title==None or len(Title)==0):
                Title="日常-"+str(WorkID)
        elif "cp_id" in Info.keys():
            WorkID=Info["rp_id"]
            WorkType="cos"
            if(Title==None or len(Title)==0):
                Title="Cosplay-"+str(WorkID)
        elif "dp_id" in Info.keys():
            WorkID=Info["rp_id"]
            WorkType="drawer"
            if(Title==None or len(Title)==0):
                Title="绘画-"+str(WorkID)
        elif "post_id" in Info.keys() and "group" in Info.keys():
            WorkID=Info["post_id"]
            WorkType="group"
            GID=Info["group"]["gid"]
            UID=Info["uid"]
            GroupName=self.LoadOrSaveGroupName(Info["group"]["name"],GID)
            if(Title==None or len(Title)==0):
                Title=GroupName+"-"+WorkID
        Title=self.LoadTitle(Title,Info)
        if SaveInfo==True:
            self.SaveInfo(Title,Info)
        WritePathRoot=os.path.join(self.SavePath,str(CoserName).replace("/","-"),str(Title).replace("/","-"))
        if Info["type"]=="larticle" and (type(Info["multi"])==bool or len(Info["multi"])==0):#Long Article. Extract URLs from HTML using regex
            match=re.findall(r"<img src=\"(.{80,100})\" alt=",Info["content"])
            URLs=list()
            if match!=None:
                for url in match:
                    temp=dict()
                    temp["type"]="image"
                    temp["path"]=url
                    URLs.append(temp)
            Info["multi"]=URLs
        for PathDict in Info["multi"]:
            URL=PathDict["path"]
            if len(URL)==0:
                continue
            FileName=os.path.split(URL)[0].split("/")[-1]
            ShouldAppendSuffix=True
            for suffix in [".jpg",".png",".jpeg",".gif"]:
                if FileName.lower().endswith(suffix):
                    ShouldAppendSuffix=False
                    break
            if ShouldAppendSuffix==True:
                FileName=FileName+".jpg"
            SavePath=os.path.join(WritePathRoot,FileName)
            ctime=Info["ctime"]
            if os.path.isfile(SavePath)==False:
                self.DownloadQueue.put([URL,WorkID,WorkType,WritePathRoot,FileName,ctime])

    def DownloadUser(self,UID,Filter):
        tmp=self.API.userWorkList(UID,Filter,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning('Found {} works UID:{} Filter: {}'.format(str(len(tmp)), str(UID), Filter))
    def DownloadGroup(self,GID):
        tmp=self.API.groupPostList(GID,Progress=self.Status,Callback=self.DownloadFromAbstractInfo)
        self.logger.warning("Found {} works for GroupGID:{}".format(str(len(tmp)),str(GID)))
    def DownloadCircle(self,CircleID,Filter):
        foo=self.API.circleList(CircleID,Filter,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} works CircleID:{} Filter:{}".format(str(len(foo)),str(CircleID),str(Filter)))
    def DownloadTag(self,Tag,Filter):
        foo=self.API.tagList(Tag,Filter,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} works Tag:{} Filter:{}".format(str(len(foo)), str(Filter), str(Tag)))
    def DownloadLikedList(self,Filter):
        foo=self.API.likedList(Filter,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} likedList Filter:{}".format(str(len(foo)), str(Filter)))
    def DownloadUserRecommends(self,UID,Filter):
        foo=self.API.userRecommends(UID,Filter,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} User Recommends UID:{} Filter:{}".format(str(len(foo)), str(UID),Filter))
    def DownloadSearch(self,Keyword,Type):
        foo=self.API.search(Keyword,Type,Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} Search Keywork:{} Type:{}".format(str(len(foo)), Keyword,Type))
    def DownloadTimeline(self):
        foo=self.API.getTimeline(Callback=self.DownloadFromAbstractInfo,Progress=self.Status)
        self.logger.warning("Found {} In Timeline".format(str(len(foo))))
    def DownloadFromAbstractInfo(self,AbstractInfo):
        '''
        通用的Callback。用于将查询获得的详细信息写入DownloadQueue
        '''
        self.QueryQueue.put(AbstractInfo)
    def DownloadFromAbstractInfoWorker(self):
        '''
        供__init__ fork新线程使用。请勿直接调用。
        本方法从QueryQueue中获取信息并查询详情后写入DownloadQueue
        '''
        while self.QueryEvent.isSet()==True:
            AbstractInfo=self.QueryQueue.get(block=True)
            Inf=None
            if self.UseCachedDetail==True:
                Inf=self.LoadCachedDetail(AbstractInfo)
            if Inf==None:
                try:
                    Inf=self.API.queryDetail(AbstractInfo)
                except:
                    self.FailedInfoList.append(AbstractInfo)
            if Inf!=None:
                    self.DownloadFromInfo(Inf)
            self.QueryQueue.task_done()
    def LoadCachedDetail(self,Info):
        '''
        用于读取本地SQL里的详细信息缓存
        '''
        Info=Info.get("detail",Info)
        ValidIDs=dict()
        for key in ["cp_id","rp_id","dp_id","ud_id","post_id"]:
            value=Info.get(key)
            if value!=None:
                ValidIDs[key]=int(value)
        Q="SELECT Info FROM WorkInfo WHERE "
        ArgsList=list()
        #Construct A List of constraints
        keys=list()
        Values=list()
        for item in ValidIDs.keys():
            keys.append(item+"=?")
            Values.append(ValidIDs[item])
        if len(keys)==0 or len(keys)!=len(Values):#Error Detection
            return None
        Q=Q+" AND ".join(keys)
        Cursor=self.InfoSQL.execute(Q,tuple(Values)).fetchall()
        if len(Cursor)==0:
            return None
        return json.loads(Cursor[0][0])
    def LoadOrSaveUserName(self,UserName,UID):
        '''
        当UserName为None时可用作纯查询函数。
        否则查询SQL里对应UID的用户名:
            如果有记录:返回原来的用户名
            否则:建立UserName和UID的记录并返回UserName
        '''
        self.InfoSQLLock.acquire()
        Q="SELECT UserName FROM UserInfo WHERE uid="+str(UID)
        Cursor=self.InfoSQL.execute(Q)
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        if UserName==None:
            self.InfoSQLLock.release()
            return None
        self.InfoSQL.execute("INSERT INTO UserInfo (uid, UserName) VALUES (?,?)",(int(UID),UserName))
        self.InfoSQLLock.release()
        return UserName
    def LoadOrSaveGroupName(self,GroupName,GID):
        '''
        当GroupName为None时可用作纯查询函数。
        否则查询SQL里对应GID的组名:
            如果有记录:返回原来的组名
            否则:建立GroupName和GID的记录并返回GroupName
        '''
        self.InfoSQLLock.acquire()
        Q="SELECT GroupName FROM GroupInfo WHERE gid="+str(GID)
        Cursor=self.InfoSQL.execute(Q)
        for item in Cursor:
            self.InfoSQLLock.release()
            return item[0]
        if GroupName==None:
            self.InfoSQLLock.release()
            return None
        self.InfoSQL.execute("INSERT INTO GroupInfo (gid, GroupName) VALUES (?,?)",(int(GID),GroupName))
        self.InfoSQLLock.release()
        return GroupName
    def LoadTitle(self,Title,Info):
        '''
        分析数据库并加载可用的标题
        如果不存在记录则不保存
        '''
        ValidIDs=dict()
        for key in ["cp_id","rp_id","dp_id","ud_id","post_id"]:
            value=Info.get(key)
            if value!=None:
                ValidIDs[key]=int(value)
        Q="SELECT Title FROM WorkInfo WHERE "
        ArgsList=list()
        keys=list()
        Values=list()
        for item in ValidIDs.keys():
            keys.append(item+"=?")
            Values.append(ValidIDs[item])
        Q=Q+" AND ".join(keys)
        Cursor=self.InfoSQL.execute(Q,tuple(Values))
        for item in Cursor:
            return item[0]
        #We Don't Save Title as it's handled by SaveInfo
        return Title
    def SaveInfo(self,Title,Info):
        '''
        保存作品信息
        '''
        self.InfoSQLLock.acquire()
        ValidIDs=dict()
        #Prepare Insert Statement
        ValidIDs["Title"]=Title
        for key in ["cp_id","rp_id","dp_id","ud_id","post_id","uid"]:
            ValidIDs[key]=int(Info.get(key,0))
        TagList=list()
        for item in Info.get("post_tags",list()):
            TagList.append(item["tag_name"])
        try:
            ValidIDs["Info"]=json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
            ValidIDs["Tags"]=json.dumps(TagList,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
        except TypeError:
            ValidIDs["Info"]=json.dumps(Info,separators=(',', ':'),ensure_ascii=False)
            ValidIDs["Tags"]=json.dumps(TagList,separators=(',', ':'),ensure_ascii=False)
        InsertQuery="INSERT OR REPLACE INTO WorkInfo ("
        keys=list()
        ValuesPlaceHolders=list()
        Values=list()
        for item in ValidIDs.keys():
            keys.append(item)
            Values.append(ValidIDs[item])
            ValuesPlaceHolders.append("?")
        InsertQuery=InsertQuery+",".join(keys)+")VALUES ("+",".join(ValuesPlaceHolders)+")"
        print (InsertQuery)
        print(Values)
        self.InfoSQL.execute(InsertQuery,tuple(Values))
        self.InfoSQLLock.release()
    def cleanup(self):
        '''
        根据Filter清理下载现场
        '''
        FolderList=list()
        for UID in self.Filter.UIDList:
            UserName=self.LoadOrSaveUserName(None,UID)
            if UserName!=None and len(UserName)>0:
                FolderPath=os.path.join(self.SavePath,UserName)
                if (os.path.normpath(FolderPath) != os.path.normpath(self.SavePath)):
                    FolderList.append(FolderPath)
        for Path in FolderList:
            if os.path.isdir(Path):
                self.logger.warning("Removing Folder"+Path)
                shutil.rmtree(Path)
        shutil.rmtree(os.path.join(self.SavePath,"DownloadTemp"))
    def cancel(self):
        '''
        取消所有任务
        保存SQL
        '''
        self.logger.warning("Clearing Thread Flags")
        self.DownloadWorkerEvent.clear()
        self.QueryEvent.clear()
        if hasattr(self,"verifyEvent"):
            self.verifyEvent.clear()
        self.QueryEvent.clear()
        self.logger.warning("Obtaining SQL Mutex")
        self.InfoSQLLock.acquire()
        self.logger.warning("Commiting SQLs")
        self.InfoSQL.commit()
        self.logger.warning("Closing SQLs")
        self.InfoSQL.close()
        self.logger.warning("Releasing Lock SQLs")
        self.InfoSQLLock.release()
        return
    def join(self):
        '''
        在下载完成前阻塞
        '''
        while self.QueryQueue.empty()==False or self.DownloadQueue.empty()==False:
            self.logger.warning ("QueryQueue Size:"+str(self.QueryQueue.qsize()))
            self.logger.warning ("DownloadQueue Size:"+str(self.DownloadQueue.qsize()))
            time.sleep(3)
    def verifyWorker(self):
        '''
        因为JSON解码也是有性能开销的。所以我们在verify()里将原始数据导入Queue.
        由单独的线程使用本函数来负责解码注入
        '''
        while self.verifyEvent.isSet()==True:
            obj=self.verifyQueue.get()
            Info=json.loads(obj)
            self.DownloadFromInfo(Info,SaveInfo=False)
            self.verifyQueue.task_done()

    def verify(self,thread=12,reverse=False):
        '''
        将SQL表里的所有数据导入下载队列查漏补缺。
        '''
        self.logger.warning("Verifying Local Cache")
        self.verifyQueue=Queue.Queue()
        self.verifyEvent=threading.Event()
        self.verifyEvent.set()
        for i in range(thread):
            t =  threading.Thread(target=self.verifyWorker)
            t.daemon = False
            t.start()
        Cursor=self.InfoSQL.execute("SELECT Info FROM WorkInfo").fetchall()
        if reverse==True:
            Cursor=reversed(Cursor)
        for item in Cursor:
            self.verifyQueue.put(item[0])
        while self.verifyQueue.empty()==False:
            self.logger.warning ("Verify Queue Size "+str(self.verifyQueue.qsize()))
            self.logger.warning ("DownloadQueue Size:"+str(self.DownloadQueue.qsize()))
            time.sleep(3)
