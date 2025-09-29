import uuid
from typing import Dict, List


class Processor:
    def __init__(self, class_name: str, proc_name: str, scheduling_strategy: str = "TIMER_DRIVEN",
                 scheduling_period: str = "1 sec", ):
        self.class_name = class_name
        self.id: str = str(uuid.uuid4())
        self.name = proc_name
        self.scheduling_strategy: str = scheduling_strategy
        self.scheduling_period: str = scheduling_period
        self.properties: Dict[str, str] = {}
        self.auto_terminated_relationships: List[str] = []

    def add_property(self, property_name: str, property_value: str):
        self.properties[property_name] = property_value

    def __repr__(self):
        return f"({self.class_name}, {self.name}, {self.properties})"

    def to_yaml_dict(self) -> dict:
        data = {
            'name': self.name,
            'id': self.id,
            'class': self.class_name,
            'scheduling strategy': self.scheduling_strategy,
            'scheduling period': self.scheduling_period,
        }
        if self.auto_terminated_relationships:
            data['auto-terminated relationships list'] = self.auto_terminated_relationships

        # The YAML format capitalizes 'Properties'
        data['Properties'] = self.properties
        return data
