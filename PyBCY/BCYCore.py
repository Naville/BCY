# -*- coding: utf-8 -*-
import base64
import json
import os
import requests
import sys
import tempfile
import threading
from Crypto.Cipher import AES

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

    所有的作品都由一组全局唯一的标识符标记。不同类型的作品所用的标识符Key不同
    自2017年7月左右起半次元API接口开启了同IP的并发请求限制。请求过快或过密会导致服务器不返回正文或者空正文。IP地址也会被Ban
    前者会触发requests.exceptions.ConnectionError异常
    '''
    APIBase = "http://api.bcy.net/api/"
    Header = {"User-Agent": "bcy/3.7.1 (iPhone; iOS 9.3.3; Scale/1.00)", "X-BCY-Version": "iOS-3.7.1"}
    APIErrorResponse = "半次元不小心滑了一跤"
    NeedLoginResponse = "阿拉，请先登录半次元"
    StatusMap = {100: "请先登录半次元",
                 4000: "请先登录半次元",
                 21: "半次元不小心滑了一跤",
                 4010:"阿拉，请先登录半次元",
                 4050:"该讨论已被管理员锁定",
                 160:"半次元在忙，请稍后再试"
                 }
    def __init__(self,Timeout=15):
        self.UID = None
        self.Token = None
        self.Crypto = AES.new(b"com_banciyuan_AI", AES.MODE_ECB)
        self.session = requests.Session()
        self.session.headers.update(BCYCore.Header)
        self.Timeout=Timeout

    def loginWithEmailAndPassWord(self, email, password):
        '''
        登录并非必须。
        但匿名模式下某些API无法使用
        '''
        data = self.POST("user/login", {"pass": password, "user": email}).content
        data = json.loads(data)["data"]
        try:
            self.UID = data["uid"]
            self.Token = data["token"]
        except TypeError:
            raise ValueError("BCYLogin Error:" + str(data))

    def EncryptData(self, Data):
        #PKCS7 from https://stackoverflow.com/a/14205319 with modifications
        if sys.version_info >= (3,0):
            Data = Data.encode("utf8")
            length = 16 - (len(Data) % 16)
            Data += bytes([length])*length
        else:
            Data = Data.decode("utf8").encode("utf8")
            length = 16 - (len(Data) % 16)
            Data += chr(length)*length
        return self.Crypto.encrypt(Data)

    def EncryptParam(self, Params):
        ParamData = None
        try:
            ParamData = json.dumps(Params, separators=(',', ':'), ensure_ascii=False, encoding='utf8')
        except TypeError:
            ParamData = json.dumps(Params, separators=(',', ':'), ensure_ascii=False)
        return {"data": base64.b64encode(self.EncryptData(ParamData))}

    def GET(self, URL, Params, **kwargs):
        return self.session.get(URL, params=Params,timeout=self.Timeout,**kwargs)

    def POST(self, URL, Params, Auth=True,Encrypt=True, **kwargs):
        if URL.startswith("http") == False:
            URL = BCYCore.APIBase + URL
        if Auth == True:
            if 'uid' not in Params.keys() and self.UID != None:
                Params['uid'] = self.UID
            if 'token' not in Params.keys() and self.Token != None:
                Params['token'] = self.Token
        if Encrypt:
            Params = self.EncryptParam(Params)
        return self.session.post(URL, data=Params,timeout=self.Timeout, **kwargs)

    def userDetail(self, UID):
        return json.loads(self.POST("user/detail", {"face": "b", "uid": UID}).content)

    def downloadWorker(self, URL, Callback=None):
        Content = None
        req = self.GET(URL, {}, stream=True)
        total_length = int(req.headers.get('Content-Length'))
        if req == None:
            return None
        if Callback == None:
            Data = req.content
            if (len(Data) == total_length):
                return Data
            else:
                return None
        Downloaded = 0
        for data in req.iter_content(chunk_size=204800):
            Downloaded += len(data)
            if Content == None:
                Content = data
            else:
                Content += data
            Callback(URL, Downloaded, total_length)
        if Downloaded == total_length:
            Callback(URL, Downloaded, total_length)
            return Content
        else:
            return None
    def coserAllWorks(self, Progress=dict(), Callback=None):
        '''
        获取所有Cos作品列表。
        '''
        minimalctime=Progress.get("coserAllWorks",0)
        items = list()
        p = 1
        while True:
            Params = {"p": p}
            p = p + 1
            foo = json.loads(self.POST("coser/allWorks", Params,Auth=False).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        Progress["coserAllWorks"]=int(items[-1]["ctime"])
                        return items
            if len(foo) == 0:
                Progress["coserAllWorks"]=int(items[-1]["ctime"])
                return items
            if int(foo[-1]["ctime"])<minimalctime:
                Progress["coserAllWorks"]=int(items[-1]["ctime"])
                return items
            items.extend(foo)

    def imageDownload(self, ImageInfo, Callback=None):
        '''
        从指定图片URL下载图片
        默认情况下下载无水印大图。失败的情况从官方API下载有水印大图
        '''
        ImageData = None
        URL = ImageInfo["url"]
        OriginalImageURL = os.path.split(URL)[0]
        try:
            ImageData = self.downloadWorker(OriginalImageURL, Callback=Callback)
        except requests.exceptions.MissingSchema:  # API有Bug.有时图片列表会出现随机文本URL
            return None
        try:
            rep = json.loads(ImageData)
            return None
        except:  # 否则说明是Document Not Found应答
            return ImageData
        ImageData = self.POST("image/download", ImageInfo).content
        rep = json.loads(ImageData)
        if type(rep["data"]) == str:
            return None
        ActualURL = rep["data"]["cover"]
        return self.downloadWorker(ActualURL, Callback=Callback)

    def followUser(self, uid):
        data = json.loads(self.POST("user/follow", {"uid": uid, "type": "dofollow"}).content)
        return int(data["status"]) == 1

    def unfollowUser(self, uid):
        data = json.loads(self.POST("user/follow", {"uid": uid, "type": "unfollow"}).content)
        return int(data["status"]) == 1

    def detailWorker(self, URL, InfoParams):
        '''
        对POST的简单封装。用于解决:
            - 关注可见
            - 已锁定内容
        '''
        data = json.loads(self.POST(URL, InfoParams).content)
        if data["status"]==4010:
            UID = int(data["data"]["profile"]["uid"])
            self.followUser(UID)
            data = json.loads(self.POST(URL, InfoParams).content)
            self.unfollowUser(UID)
        if data["status"]==4050:
            return None
        return data["data"]

    def illustDetail(self, dp_id, rp_id):
        return self.detailWorker("illust/detail", {"dp_id": str(dp_id), "rp_id": str(rp_id)})

    def cosDetail(self, cp_id, rp_id):
        return self.detailWorker("coser/detail", {"rp_id": str(rp_id), "cp_id": str(cp_id)})

    def dailyDetail(self, ud_id):
        return self.detailWorker("daily/detail", {"ud_id": str(ud_id)})

    def groupPostDetail(self, GroupID, PostID):
        return self.detailWorker("group/postDetail", {"gid": GroupID, "post_id": PostID})
    def recommendWork(self, WorkType, Info):
        '''
        推荐某个作品。Info为*只*包含标识符的字典
        '''
        Info["token"]=self.Token
        return json.loads(self.POST(WorkType + "/doTuijian", Info,Auth=False).content)

    def unrecommendWork(self, WorkType, Info):
        '''
        取消推荐某个作品。Info为*只*包含标识符的字典
        '''
        Info["token"]=self.Token
        return json.loads(self.POST(WorkType + "/undoTuijian", Info,Auth=False).content)


    def likeWork(self, WorkType, Info):
        '''
        给某个作品点赞。Info为*只*包含标识符的字典
        '''
        if WorkType=="daily":
            Info["token"]=self.Token
            return json.loads(self.POST("daily/doPostDing", Info,Auth=False).content)
        else:
            Info["token"]=self.Token
            return json.loads(self.POST(WorkType + "/doZan", Info,Auth=False).content)

    def unlikeWork(self, WorkType, Info):
        '''
        取消给某个作品点赞。Info为*只*包含标识符的字典
        '''
        if WorkType=="daily":
            Info["token"]=self.Token
            return json.loads(self.POST("daily/undoPostDing", Info,Auth=False).content)
        else:
            Info["token"]=self.Token
            return json.loads(self.POST(WorkType + "/undoZan", Info,Auth=False).content)

    def queryDetail(self, Info):
        '''
        这个方法为作品的详情查询封装。
        分析Info后分发给相应类型的API并返回结果
        大多数时候你应该调用这个而不是直接调用底层API
        '''

        Info = Info.get("detail",Info)
        if "ud_id" in Info.keys():
            return self.dailyDetail(Info["ud_id"])
        elif "cp_id" in Info.keys():
            return self.cosDetail(Info["cp_id"], Info["rp_id"])
        elif "dp_id" in Info.keys():
            return self.illustDetail(Info["dp_id"], Info["rp_id"])
        elif "post_id" in Info.keys() and "gid" in Info.keys():
            return self.groupPostDetail(Info["gid"], Info["post_id"])
        else:
            with tempfile.NamedTemporaryFile(delete=False, suffix="PyBCYDetail-") as outfile:
                json.dump(Info, outfile)
                raise Exception('QueryDetail is called with unsupported Info. Written To:', outfile.name)

    def WorkTypeCircle(self, name):
        '''
        name是网站banner上的名称，目前有:
            绘画
            COS
            写作
            漫展
            讨论
        '''
        return json.loads(self.POST("tag/relaCircle", {"name": name}).content)

    def report(self, Info, reportType, WorkType, Reason):
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
        Args = dict()
        Args.update(Info)
        Args["type"] = Reason
        return self.POST(WorkType + "/denounce" + str(reportType), Args)

    def tagStatus(self, TagName):
        return json.loads(self.POST("tag/status", {"name": TagName}).content)

    # 以下都为列表类应答API
    def getReply(self, WorkType, Params, **kwargs):
        '''
        获取某个作品的回复列表
        Params是作品标识符字典
        '''
        items = list()
        p = 1
        while True:
            Par = {"p": p}
            Par.update(Params)
            p = p + 1
            foo = json.loads(self.POST(WorkType + "/getReply", Par).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)

    def goodsList(self, CircleID, Progress=dict(), Callback=None):
        '''
        获取某个圈子相关的周边列表。
        不支持迭代状态保存
        '''
        items = list()
        p = 1
        while True:
            Params = {"id": str(CircleID), "p": p, "limit": 50}
            p = p + 1
            foo = json.loads(self.POST("goods/listHotCore", Params).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)

    def tagDiscussionList(self, TagName, Progress=dict(), Callback=None):
        '''
        获取某个Tag相关的串列表。
        不支持迭代状态保存
        '''
        items = list()
        p = 1
        while True:
            Params = {"p": p, "filter": "group", "name": TagName, "limit": 50}
            p = p + 1
            foo = json.loads(self.POST("tag/detail", Params).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)

    def search(self, keyword, type, Progress=None, Callback=None, *kwargs):
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
        items = list()
        p = 1
        while True:
            itemList = json.loads(
                self.POST("search/search" + str(type), {"query": keyword, "p": p}).content)[
                "data"]["results"]
            p = p + 1
            if len(itemList) == 0:
                return items
            items.extend(itemList)
            if Callback != None:
                for item in itemList:
                    if Callback(item)==True:
                        return items

    def circleList(self, CircleID, Filter, Progress=dict(), Callback=None):
        since = 0
        firstRun = False
        latestCTIME = Progress.get("circle/work" + str(CircleID) + Filter, None)
        if latestCTIME == None:
            firstRun = True
            latestCTIME = 0
        items = list()
        while True:
            Params = {"id": CircleID, "filter": Filter, "since": since}
            foo = json.loads(self.POST("circle/work", Params).content)["data"]
            shouldBreak = False  # if ctime <= our stop point,break
            for i in foo:
                if Callback != None:
                    if Callback(i)==True:
                        return items
                if firstRun == False and int(i["ctime"]) < latestCTIME:
                    shouldBreak = True
                if int(i["ctime"]) > latestCTIME:
                    Progress["circle/work" + str(CircleID) + Filter] = int(i["ctime"])
                if since == 0 or int(i["ctime"]) < since:
                    since = int(i["ctime"])
            if len(foo) == 0 or shouldBreak:
                return items
            items.extend(foo)

    def tagList(self, TagName, Filter, Progress=dict(), Callback=None):
        since = 0
        firstRun = False
        latestCTIME = Progress.get("circle/tag" + str(TagName) + Filter, None)
        if latestCTIME == None:
            firstRun = True
            latestCTIME = 0
        items = list()
        while True:
            Params = {"name": TagName, "filter": Filter, "since": since}
            foo = json.loads(self.POST("circle/tag", Params).content)["data"]
            shouldBreak = False  # if ctime <= our stop point,break
            for i in foo:
                if Callback != None:
                    if Callback(i)==True:
                        return items
                if firstRun == False and int(i["ctime"]) < latestCTIME:
                    shouldBreak = True
                if int(i["ctime"]) > latestCTIME:
                    Progress["circle/tag" + str(TagName) + Filter] = int(i["ctime"])
                if since == 0 or int(i["ctime"]) < since:
                    since = int(i["ctime"])
            if len(foo) == 0 or shouldBreak:
                return items
            items.extend(foo)

    def groupPostList(self, GroupID, Progress=dict(), Callback=None):
        '''
        这个是用来给定一个讨论串用来获取所有回贴列表的。例如41709对应https://bcy.net/group/list/41709
        '''
        items = list()
        p = 1
        while True:
            Params = {"p": p, "gid": str(GroupID), "type": "ding", "limit": 50}
            p = p + 1
            foo = json.loads(self.POST("group/listPosts", Params).content)["data"]
            if Callback != None:
                for i in foo:
                    if Callback(i)==True:
                        return items
            if len(foo) == 0:
                return items
            items.extend(foo)
    def getTimeline(self,Progress=dict(),Callback=None):
        '''
        获取当前用户的推送列表
        可在https://bcy.net/home/account/settings的首页设置下进行设置
        '''
        since = 0
        items = list()
        try:
            LAST_TL_ID = Progress.get("getTimeline", None)
            Param = {"num": 50,"type": "square"}
            Latest = json.loads(self.POST("timeline/latest", Param).content)["data"]
            items.extend(Latest)
            if len(items) == 0:
                return items
            for x in items:
                if Callback != None:
                    if Callback(x)==True:
                        return items
                if LAST_TL_ID != None:
                    if int(x["tl_id"]) < LAST_TL_ID:
                        Progress["getTimeline"] = int(items[0]["tl_id"])
                        return items
            while True:
                since = int(items[-1]["tl_id"])  # 严格意义上来说我们需要比较并找出最小的tl_id。但是官方的应答是已经排序好的。所以官方App直接取最后一个。我们照抄
                Param["since"] = str(since)
                more = json.loads(self.POST("timeline/more", Param).content)["data"]
                items.extend(more)
                if len(more) == 0:
                    Progress["getTimeline"] = int(items[0]["tl_id"])
                    return items
                for x in more:
                    if Callback != None:
                        if Callback(x)==True:
                            return items
                    if LAST_TL_ID != None:
                        if int(x["tl_id"]) < LAST_TL_ID:
                            Progress["getTimeline"] = int(items[0]["tl_id"])
                            return items
        except:
            return items


    def userWorkList(self, UID, Filter, Progress=dict(), Callback=None):
        '''
        获取某个用户的所有作品
        '''
        since = 0
        items = list()
        if (Filter=="daily"):#API设计缺陷
            Filter="user"
        try:
            LAST_TL_ID = Progress.get("timeline/latest" + str(UID) + Filter, None)
            Param = {"uid": str(UID), "num": 50, "filter": "origin", "source": Filter, "type": "user"}
            Latest = json.loads(self.POST("timeline/latest", Param).content)["data"]
            items.extend(Latest)
            if len(items) == 0:
                return items
            for x in items:
                if Callback != None:
                    if Callback(x)==True:
                        return items
                if LAST_TL_ID != None:
                    if int(x["tl_id"]) < LAST_TL_ID:
                        Progress["timeline/latest" + str(UID) + Filter] = int(items[0]["tl_id"])
                        return items
            while True:
                since = int(items[-1]["tl_id"])  # 严格意义上来说我们需要比较并找出最小的tl_id。但是官方的应答是已经排序好的。所以官方App直接取最后一个。我们照抄
                Param["since"] = str(since)
                more = json.loads(self.POST("timeline/more", Param).content)["data"]
                items.extend(more)
                if len(more) == 0:
                    Progress["timeline/latest" + str(UID) + Filter] = int(items[0]["tl_id"])
                    return items
                for x in more:
                    if Callback != None:
                        if Callback(x)==True:
                            return items
                    if LAST_TL_ID != None:
                        if int(x["tl_id"]) < LAST_TL_ID:
                            Progress["timeline/latest" + str(UID) + Filter] = int(items[0]["tl_id"])
                            return items
        except:
            return items

    def likedList(self, Filter, Progress=None, Callback=None):
        '''
        下载自己点过赞的作品
        不支持迭代状态保存
        '''
        items = list()
        p = 1
        try:
            while True:
                itemList = json.loads(self.POST("space/zanlist", {"sub": Filter, "p": p}).content)[
                    "data"]
                p = p + 1
                if len(itemList) == 0:
                    return items
                items.extend(itemList)
                if Callback != None:
                    for item in itemList:
                        if Callback(item)==True:
                            return items
        except:
            return items

    def userRecommends(self, UID, Filter, Callback=None, Progress=dict()):
        '''
        下载其他某个用户赞过的作品
        '''
        if (Filter=="daily"):#API设计缺陷
            Filter="user"
        since = 0
        items = list()
        LAST_TL_ID = Progress.get("timeline/latest" + str(UID) + Filter, None)
        Param = {"uid": str(UID), "filter": "tuijian", "source": Filter}
        Latest = json.loads(self.POST("timeline/userGrid", Param).content)["data"]
        items.extend(Latest)
        if len(items) == 0:
            return items
        for x in items:
            if Callback != None:
                if Callback(x)==True:
                    return items
            if LAST_TL_ID != None:
                if int(x["tl_id"]) < LAST_TL_ID:
                    Progress["timeline/userGrid" + str(UID) + Filter] = int(items[0]["tl_id"])
                    return items
        while True:
            since = int(items[-1]["tl_id"])  # 严格意义上来说我们需要比较并找出最小的tl_id。但是官方的应答是已经排序好的。所以官方App直接取最后一个。我们照抄
            Param["since"] = since
            more = json.loads(self.POST("timeline/userGrid", Param).content)["data"]
            items.extend(more)
            if len(more) == 0:
                Progress["timeline/userGrid" + str(UID) + Filter] = int(items[0]["tl_id"])
                return items
            for x in more:
                if Callback != None:
                    if Callback(x)==True:
                        return items
                if LAST_TL_ID != None:
                    if int(x["tl_id"]) < LAST_TL_ID:
                        Progress["timeline/userGrid" + str(UID) + Filter] = int(items[0]["tl_id"])
                        return items
