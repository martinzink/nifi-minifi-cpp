from ..core.ControllerService import ControllerService


class ElasticsearchCredentialsService(ControllerService):
    def __init__(self, name=None):
        super(ElasticsearchCredentialsService, self).__init__(name=name)

        self.service_class = 'ElasticsearchCredentialsControllerService'
        self.properties['Credentials Type'] = "Basic authentication"
        self.properties['Username'] = "elastic"
        self.properties['Password'] = "password"

