from textwrap import dedent

from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.docker_container_builder import DockerContainerBuilder
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext


class HttpProxy(Container):
    def __init__(self, test_context: MinifiTestContext):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                """.format(base_image='ubuntu/squid:5.2-22.04_beta', proxy_username='admin', proxy_password='test101', proxy_port='3128'))

        builder = DockerContainerBuilder(
            image_tag="minifi-http-proxy:latest",
            dockerfile_content=dockerfile
        )
        builder.build()

        super().__init__("minifi-http-proxy:latest", f"http-proxy-{test_context.scenario_id}", test_context.network)

    def deploy(self):
        super().deploy()
        finished_str = "Accepting HTTP Socket connections at"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=5,
            bail_condition=lambda: self.exited,
            context=None
        )

    def check_http_proxy_access(self, url):
        (code, output) = self.exec_run(["cat", "/var/log/squid/access.log"])
        return code == 0 and url.lower() in output.lower() \
            and ((output.count("TCP_DENIED") != 0
                 and output.count("TCP_MISS") >= output.count("TCP_DENIED"))
                 or output.count("TCP_DENIED") == 0 and "TCP_MISS" in output)
