import re,logging
class BCYDownloadFilter(object):
    def __init__(self):
        '''
        Download Filter used by BCYDownloadUtils
        Every list supports compiled regex unless otherwise specified
        - UIDList is a list of int
        - WorkList
        - TagList
        - UserNameList
        - RegexList is a list of compiled regexs running against json dumped info
        '''
        self.UIDList=list()
        self.WorkList=list()
        self.TagList=list()
        self.UserNameList=list()
        self.RegexList=list()
        self.logger=logging.getLogger(str(__name__)+"-"+str(hex(id(self))))
    def ShouldBlockInfo(self,Info):
        for UID in self.UIDList:
            if int(Info['uid'])==UID:
                self.logger.debug("%s Filted due to its uid:%i"%(Info,int(Info['uid'])))
                return True
        for Tag in self.TagList:
            for item in Info["post_tags"]:
                if Tag==item:
                    self.logger.debug("%s Filted due to its Tag:%s"%(Info,item))
                    return True
                elif type(Tag)==type(re.compile("")) and Tag.match(item):
                    self.logger.debug("%s Filted due to it matching regex Tag:%s"%(Info,Tag))
                    return True
        for UserName in self.UserNameList:
            if UserName==Info["profile"]["uname"]:
                self.logger.debug("%s Filted due to its UserName:%s"%(Info,UserName))
                return True
            elif type(UserName)==type(re.compile("")) and UserName.match(Info["profile"]["uname"])!=None:
                self.logger.debug("%s Filted due to it matching regex UserName:%s"%(Info,UserName))
                return True
        for R in self.RegexList:
            if(R.match(json.dumps(Info,separators=(',', ':'),ensure_ascii=False, encoding='utf8'))!=None):
                self.logger.debug("%s Filted due to its info matching regex:%s"%(Info,R))
                return True
        return False
