from abc import ABC, abstractmethod

import sheen.skill as skill


class ICapture(skill.ISkill, ABC):
    ...


class Capture(ICapture):
    def define(self):
        pass
