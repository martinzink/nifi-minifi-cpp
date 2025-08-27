from pydoc import locate
from minifi.controllers.SSLContextService import SSLContextService

def create_processor(context, processor_type, processor_name, property_name, property_value, container_name, engine='minifi-cpp'):
    container = context.test.acquire_container(context=context, name=container_name, engine=engine)
    processor = locate("minifi.processors." + processor_type + "." + processor_type)(context=context)
    processor.set_name(processor_name)
    if property_name is not None:
        processor.set_property(property_name, property_value)
    context.test.add_node(processor)
    # Assume that the first node declared is primary unless specified otherwise
    if not container.get_start_nodes():
        container.add_start_node(processor)

def setUpSslContextServiceForProcessor(context, processor_name: str):
    minifi_crt_file = '/tmp/resources/minifi_client.crt'
    minifi_key_file = '/tmp/resources/minifi_client.key'
    root_ca_crt_file = '/tmp/resources/root_ca.crt'
    ssl_context_service = SSLContextService(cert=minifi_crt_file, ca_cert=root_ca_crt_file, key=minifi_key_file)
    processor = context.test.get_node_by_name(processor_name)
    processor.controller_services.append(ssl_context_service)
    processor.set_property("SSL Context Service", ssl_context_service.name)