# -*- coding: utf-8 -*-
import requests,base64,logging,json,os,time,tempfile,sys,logging,re
from Crypto.Cipher import AES
from pkcs7 import PKCS7Encoder
from copy import deepcopy
try:
    reload(sys)
    sys.setdefaultencoding('utf8')
except:
    pass
class BCYCore(object):
    '''
    对所有列表类API而言,我们在Progress里存入本次迭代所找到的最新作品时间戳或其他标志位。下次迭代时遇到比该作品更旧的作品时说明到头了可跳出。
    可能的Filter值:
         - coser
         - all
         - drawer
         - writer
         - daily
         - zhipin
    可能的WorkType值:
         - illust
         - coser
         - daily
         - group
         - circle
         - group

    Each work,regardless of WorkType, is globally identified using a set of Keys & Values
    Different WorkType of work has different set of used keys.Refer to queryDetail()'s code
    Most APIs use WorkType in URL to identifier WorkType and respond accordingly
    '''
    APIBase="http://api.bcy.net/api/"
    Header={"User-Agent":"bcy/3.8.0 (iPad; iOS 9.3.3; Scale/2.00)","X-BCY-Version":"iOS-3.8.0"}
    NeedFollowServerResponse="阿拉，请先关注一下嘛"
    LockedServerResponse="该作品已被管理员锁定"
    APIErrorResponse="半次元不小心滑了一跤"
    GroupLockedResponse="该讨论已被管理员锁定"
    StatusMap={100:"请先登录半次元",
               4000:"请先登录半次元",
               21:"半次元不小心滑了一跤"
                }
    def __init__(self,Timeout=20):
        self.UID=None
        self.Token=None
        # Lack Support of PKCS7Padding.Solve it in actual crypto stage
        self.Crypto=AES.new('com_banciyuan_AI', AES.MODE_ECB)
        self.pkcs7=PKCS7Encoder()
        self.Timeout=Timeout
        self.session=requests.Session()
        self.session.headers.update(BCYCore.Header)
    def loginWithEmailAndPassWord(self,email,password):
        '''
        Login is not required.
        However certain operations like viewing Follower-Only content might fail in anonymous mode
        '''
        data=self.POST("user/login",{"pass":password,"user":email},timeout=self.Timeout).content
        data=json.loads(data)["data"]
        try:
            self.UID=data["uid"]
            self.Token = data["token"]
        except:
            raise ValueError("BCYLogin Error:"+str(data))
    def EncryptData(self,Data):
        Data=Data.encode("utf8")
        Data=self.pkcs7.encode(Data)
        return self.Crypto.encrypt(Data)
    def EncryptParam(self,Params):
        ParamData=None
        try:
            ParamData=json.dumps(Params,separators=(',', ':'),ensure_ascii=False, encoding='utf8')
        except TypeError:
            ParamData=json.dumps(Params,separators=(',', ':'),ensure_ascii=False)
        return {"data":base64.b64encode(self.EncryptData(ParamData))}
    def GET(self,URL,Params,**kwargs):
        try:
            return self.session.get(URL,params=Params,**kwargs)
        except:
            return None

    def POST(self,URL,Params,Auth=True,**kwargs):
        if URL.startswith(BCYCore.APIBase)==False:
                URL=BCYCore.APIBase + URL
        if Auth==True:
            if 'uid' not in Params.keys() and self.UID!=None:
                Params['uid']=self.UID
            if 'token' not in Params.keys() and self.Token!=None:
                Params['token']=self.Token
        P=self.EncryptParam(Params)
        try:
            return self.session.post(URL,data=P,**kwargs)
        except:
            return None
    def userDetail(self,UID):
        try:
            return json.loads(self.POST("user/detail",{"face":"b","uid":UID},timeout=self.Timeout).content)
        except:
            return None
    def downloadWorker(self,URL,Callback=None):
        Content=None
        req=self.GET(URL,{},timeout=self.Timeout,stream=True)
        total_length = int(req.headers.get('Content-Length'))
        if req==None:
            return None
        if Callback==None:
            Data=req.content
            if(len(Data)==total_length):
                return Data
            else:
                return None
        Downloaded = 0
        for data in req.iter_content(chunk_size=204800):
            Downloaded += len(data)
            if Content==None:
                Content=data
            else:
                Content+=data
            Callback(URL,Downloaded,total_length)
        if Downloaded==total_length:
            Callback(URL,Downloaded,total_length)
            return Content
        else:
            return None

    def imageDownload(self,ImageInfo,Callback=None):
        ImageData=None
        URL=ImageInfo["url"]
        if URL.startswith("http")==False:
            return None
        OriginalImageURL=os.path.split(URL)[0]
        try:
            ImageData=self.downloadWorker(OriginalImageURL,Callback=Callback)
        except:
            pass
        if ImageData==None:#API Endpoint Bug Sometimes Results In Non-Existing URL In Detail Response
            return None
        try:
            rep=json.loads(ImageData)
        except ValueError:
            return ImageData
        ImageData=self.POST("image/download",ImageInfo,timeout=self.Timeout)
        if ImageData==None:#API Endpoint Bug Sometimes Results In Non-Existing URL In Detail Response
            return None
        ImageData=ImageData.content
        try:
            rep=json.loads(ImageData)
            ActualURL=rep["data"]["cover"]
            return self.downloadWorker(ActualURL,Callback=Callback)
        except:
            return None
        return None
    def followUser(self,uid):
        try:
            data=json.loads(self.POST("user/follow",{"uid":uid,"type":"dofollow"},timeout=self.Timeout).content)
            return int(data["status"])==1
        except:
            return False
    def unfollowUser(self,uid):
        try:
            data=json.loads(self.POST("user/follow",{"uid":uid,"type":"unfollow"},timeout=self.Timeout).content)
            return int(data["status"])==1
        except:
            return False
    def detailWorker(self,URL,InfoParams):
        '''
        This is a wrapper around POST() to solve various overheads like:
            Locked Work
            Follower-Only Content
        '''
        data=None
        try:
            data=json.loads(self.POST(URL,InfoParams,timeout=self.Timeout).content)
        except:
            return None
        UnicodeStatus=None
        try:
            UnicodeStatus=unicode(data["data"]["data"]).encode('utf8')
        except:
            try:
                UnicodeStatus=data["data"]["data"]
            except:
                pass
        if UnicodeStatus==BCYCore.NeedFollowServerResponse:
            UID=int(data["data"]["profile"]["uid"])
            self.followUser(UID)
            try:
                data=json.loads(self.POST(URL,InfoParams,timeout=self.Timeout).content)
            except:
                self.unfollowUser(UID)
                return None
            self.unfollowUser(UID)
        if UnicodeStatus==BCYCore.LockedServerResponse:
            return None
        return data["data"]

    def illustDetail(self,dp_id,rp_id):
        return self.detailWorker("illust/detail",{"dp_id":str(dp_id),"rp_id":str(rp_id)})
    def cosDetail(self,cp_id,rp_id):
        return self.detailWorker("coser/detail",{"rp_id":str(rp_id),"cp_id":str(cp_id)})
    def dailyDetail(self,ud_id):
        return self.detailWorker("daily/detail",{"ud_id":str(ud_id)})
    def groupPostDetail(self,GroupID,PostID):
        return self.detailWorker("group/postDetail",{"gid":GroupID,"post_id":PostID})
    def likeWork(self,WorkType,Info):
        '''
        Info is the WorkInfo, containing WorkIdentifiers *ONLY*
        '''
        return self.POST(WorkType+"/doZan",Info)
    def unlikeWork(self,WorkType,Info):
        '''
        Info is the WorkInfo, containing WorkIdentifiers *ONLY*
        '''
        return self.POST(undoZan+"/undoZan",Info)
    def queryDetail(self,Info):
        '''
        This method:
            1. Analyze Info
            2. Extract Values and dispatch them to corresponding APIs
            3. Return the result
        It's usually better to call this method instead of directly using the underlying APIs
        '''
        try:
            Info=Info["detail"]
        except:
            pass
        if "ud_id" in Info.keys():
            return self.dailyDetail(Info["ud_id"])
        elif "cp_id" in Info.keys():
            return self.cosDetail(Info["cp_id"],Info["rp_id"])
        elif "dp_id" in Info.keys():
            return self.illustDetail(Info["dp_id"],Info["rp_id"])
        elif "post_id" in Info.keys() and "gid" in Info.keys():
            return self.groupPostDetail(Info["gid"],Info["post_id"])
        else:
            with tempfile.NamedTemporaryFile(delete=False, suffix="PyBCYDetail-") as outfile:
                json.dump(Info, outfile)
                raise Exception('QueryDetail is called with unsupported Info. Written To:', outfile.name)
    def WorkTypeCircle(self,name):
        '''
        name is the column name used on the website banner, possible values:
            绘画
            COS
            写作
            漫展
            讨论
        '''
        try:
            return json.loads(self.POST("tag/relaCircle",{"name":name},timeout=self.Timeout).content)
        except:
            return None
    def report(self,Info,reportType,WorkType,Reason):
        '''
        Reason 是全局使用的那些理由。目前有:
            不宜公开讨论的政治问题
            广告等垃圾信息
            不友善内容
            色情／血腥／暴力等违规内容
            盗用／抄袭他人作品
            其他
        reportType为举报类型.可为:
            Reply
            Post
            ReplyComment
        分别需要于Info传入对应的标识符
        '''
        Args=dict()
        Args.update(Info)
        Args["type"]=Reason
        return self.POST(WorkType+"/denounce"+str(reportType),Args)
    def tagStatus(self,TagName):
        return json.loads(self.POST("tag/status",{"name":TagName}).content)

# 以下都为列表类应答API
    def getReply(self,WorkType,Params,**kwargs):
        '''
        获取某个作品的回复列表
        Params是作品标识符字典
        '''
        items=list()
        p=1
        while True:
            Par={"p":p}
            Par.update(Params)
            p=p+1
            foo=json.loads(self.POST(WorkType+"/getReply",Par).content)["data"]
            if Callback!=None:
                for i in foo:
                    Callback(i)
            if len(foo)==0:
                return items
            items.extend(foo)
        return items
    def goodsList(self,CircleID,Progress=dict(),Callback=None):
        '''
        获取某个圈子相关的周边列表。
        不支持迭代状态保存
        '''
        items=list()
        p=1
        while True:
            Params={"id":str(GroupID),"p":p,"limit":20}
            p=p+1
            foo=json.loads(self.POST("goods/listHotCore",Params).content)["data"]
            if Callback!=None:
                for i in foo:
                    Callback(i)
            if len(foo)==0:
                return items
            items.extend(foo)
        return items
    def tagDiscussionList(self,TagName,Progress=dict(),Callback=None):
        '''
        获取某个Tag相关的串列表。
        不支持迭代状态保存
        '''
        items=list()
        p=1
        while True:
            Params={"p":p,"filter":"group","name":TagName,"limit":20}
            p=p+1
            foo=json.loads(self.POST("tag/detail",Params).content)["data"]
            if Callback!=None:
                for i in foo:
                    Callback(i)
            if len(foo)==0:
                return items
            items.extend(foo)
        return items
    def search(self,keyword,type,Progress=None,Callback=None,*kwargs):
        '''
        type的可能值:
                Cos
                Tags
                Works
                Daily
                Illust
                Goods
                Novel
                Post
                Content
                User
                Group
                All
        不支持迭代状态保存
        '''
        items=list()
        p=1
        while True:
            try:
                itemList=json.loads(self.POST("search/search"+str(type),{"query":keyword,"p":p}).content)["data"]["results"]
                p=p+1
                if len(itemList)==0:
                    return items
                items.extend(itemList)
                if Callback!=None:
                    for item in itemList:
                        Callback(item)
            except:
                return items
    def circleList(self,CircleID,Filter,Progress=dict(),Callback=None):
        since=0
        firstRun=False
        latestCTIME=Progress.get("circle/work"+str(CircleID)+Filter,None)
        if latestCTIME==None:
            firstRun=True
            latestCTIME=0
        items=list()
        while True:
            Params={"id":CircleID,"filter":Filter,"since":since}
            foo=json.loads(self.POST("circle/work",Params).content)["data"]
            shouldBreak=False#if ctime <= our stop point,break
            for i in foo:
                if Callback!=None:
                    Callback(i)
                if firstRun==False and int(i["ctime"])<latestCTIME:
                    shouldBreak=True
                if int(i["ctime"])>latestCTIME:
                    Progress["circle/work"+str(CircleID)+Filter]=int(i["ctime"])
                if since==0 or int(i["ctime"])<since:
                    since=int(i["ctime"])
            if len(foo)==0 or shouldBreak:
                return items
            items.extend(foo)

        return items
    def tagList(self,TagName,Filter,Progress=dict(),Callback=None):
        since=0
        firstRun=False
        latestCTIME=Progress.get("circle/tag"+str(TagName)+Filter,None)
        if latestCTIME==None:
            firstRun=True
            latestCTIME=0
        items=list()
        while True:
            Params={"name":TagName,"filter":Filter,"since":since}
            foo=json.loads(self.POST("circle/tag",Params).content)["data"]
            shouldBreak=False#if ctime <= our stop point,break
            for i in foo:
                if Callback!=None:
                    Callback(i)
                if firstRun==False and int(i["ctime"])<latestCTIME:
                    shouldBreak=True
                if int(i["ctime"])>latestCTIME:
                    Progress["circle/tag"+str(TagName)+Filter]=int(i["ctime"])
                if since==0 or int(i["ctime"])<since:
                    since=int(i["ctime"])
            if len(foo)==0 or shouldBreak:
                return items
            items.extend(foo)

        return items
    def groupPostList(self,GroupID,Progress=dict(),Callback=None):
        '''
        这个是用来给定一个讨论串用来获取所有回贴列表的。例如41709对应https://bcy.net/group/list/41709
        '''
        items=list()
        p=1
        while True:
            Params={"p":p,"gid":str(GroupID),"type":"ding","limit":20}
            p=p+1
            foo=json.loads(self.POST("group/listPosts",Params).content)["data"]
            if Callback!=None:
                for i in foo:
                    Callback(i)
            if len(foo)==0:
                return items
            items.extend(foo)

        return items
    def userWorkList(self,UID,Filter,Progress=dict(),Callback=None):
        since=0
        items=list()
        LAST_TL_ID=Progress.get("timeline/latest"+str(UID)+Filter,None)
        Param={"uid":str(UID),"num":10,"filter":"origin","source":Filter,"type":"user"}
        try:
            Latest=json.loads(self.POST("timeline/latest",Param).content)["data"]
            items.extend(Latest)
            if len(items)==0:
                return items
            if LAST_TL_ID!=None:
                for x in items:
                    if int(x["tl_id"])<LAST_TL_ID:
                        Progress["timeline/latest"+str(UID)+Filter]=int(items[0]["tl_id"])
                        return items
            while True:
                since=int(items[-1]["tl_id"])#严格意义上来说我们需要比较并找出最小的tl_id。但是官方的应答是已经排序好的。所以官方App直接取最后一个。我们照抄
                Param["since"]=str(since)
                more=json.loads(self.POST("timeline/more",Param).content)["data"]
                items.extend(more)
                if len(more)==0:
                    Progress["timeline/latest"+str(UID)+Filter]=int(items[0]["tl_id"])
                    return items
                if LAST_TL_ID!=None:
                    for x in more:
                        if int(x["tl_id"])<LAST_TL_ID:
                            Progress["timeline/latest"+str(UID)+Filter]=int(items[0]["tl_id"])
                            return items

        except:
            return items
    def likedList(self,Filter,Progress=None,Callback=None):
        '''
        下载自己点过赞的作品
        不支持迭代状态保存
        '''
        items=list()
        p=1
        while True:
            try:
                itemList=json.loads(self.POST("space/zanlist",{"sub":Filter,"p":p}).content)["data"]
                p=p+1
                if len(itemList)==0:
                    return items
                items.extend(itemList)
                if Callback!=None:
                    for item in itemList:
                        Callback(item)

            except:
                return items
    def userRecommends(self,UID,Filter,Callback=None,Progress=dict()):
        '''
        下载其他某个用户赞过的作品
        '''
        since=0
        items=list()
        LAST_TL_ID=Progress.get("timeline/latest"+str(UID)+Filter,None)
        Param={"uid":str(UID),"filter":"tuijian","source":Filter}
        try:
            Latest=json.loads(self.POST("timeline/userGrid",Param).content)["data"]
            items.extend(Latest)
            if len(items)==0:
                return items
            if LAST_TL_ID!=None:
                for x in items:
                    if int(x["tl_id"])<LAST_TL_ID:
                        Progress["timeline/userGrid"+str(UID)+Filter]=int(items[0]["tl_id"])
                        return items
            while True:
                since=int(items[-1]["tl_id"])#严格意义上来说我们需要比较并找出最小的tl_id。但是官方的应答是已经排序好的。所以官方App直接取最后一个。我们照抄
                Param["since"]=since
                more=json.loads(self.POST("timeline/userGrid",Param).content)["data"]
                items.extend(more)
                if len(more)==0:
                    Progress["timeline/userGrid"+str(UID)+Filter]=int(items[0]["tl_id"])
                    return items
                if LAST_TL_ID!=None:
                    for x in more:
                        if int(x["tl_id"])<LAST_TL_ID:
                            Progress["timeline/userGrid"+str(UID)+Filter]=int(items[0]["tl_id"])
                            return items

        except:
            return items
