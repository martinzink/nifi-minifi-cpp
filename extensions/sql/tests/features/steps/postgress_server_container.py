from textwrap import dedent
from minifi_test_framework.containers.container import Container
from minifi_test_framework.containers.docker_container_builder import DockerContainerBuilder
from minifi_test_framework.core.helpers import wait_for_condition


class PostgresContainer(Container):
    def __init__(self, context):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN mkdir -p /docker-entrypoint-initdb.d
                RUN echo "#!/bin/bash" > /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "set -e" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "psql -v ON_ERROR_STOP=1 --username "postgres" --dbname "postgres" <<-EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table (int_col INTEGER, text_col TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (1, 'apple');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (2, 'banana');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (3, 'pear');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table2 (int_col INTEGER, \\"tExT_Col\\" TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (5, 'ApPlE');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (6, 'BaNaNa');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh
                """.format(base_image='postgres:17.4'))
        builder = DockerContainerBuilder(
            image_tag="minifi-postgres-server:latest",
            dockerfile_content=dockerfile
        )
        builder.build()

        super(PostgresContainer, self).__init__("minifi-postgres-server:latest", f"postgres-server-{context.scenario_id}", context.network)
        self.environment = ["POSTGRES_PASSWORD=password"]

    def deploy(self) -> bool:
        super(PostgresContainer, self).deploy()
        finished_str = "database system is ready to accept connections"
        return wait_for_condition(
            condition=lambda: finished_str in self.get_logs(),
            timeout_seconds=5,
            bail_condition=lambda: self.exited,
            context=None)

    def check_query_results(self, query, number_of_rows):
        (code, output) = self.exec_run(["psql", "-U", "postgres", "-c", query])
        return code == 0 and str(number_of_rows) + " rows" in output
