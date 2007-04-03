class BCYDownloadFilter(object):
    def __init__(self):
        self.RegexList=list()
        self.UIDList=list()
        self.WorkList=list()
        self.TagList=list()
    def ShouldBlockInfo(self,Info):
        for UID in self.UIDList:
            if int(Info['uid'])==UID:
                return True
        for Tag in self.TagList:
            for item in Info["post_tags"]:
                if Tag==item:
                    return True
        return False
