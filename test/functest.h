// A simple smoke test to verify that the platform's compiler isn't sticking
// padding into the structures. If it is, this indicates that this platform
// needs to be special cased, and the structs re-defined to work.
bool TestStructSizes();

// A test to validate the request creation and validation functions
bool TestProtocolCreateRequest();

// A test to validate the response creation and validation functions
bool TestProtocolCreateResponse();

// A test to validate server calculation of message size
bool TestServerCalculateMessageSize();

// A test to validate server generation of peers response messages
bool TestServerGeneratePeersResponse();

// A test to validate server generation of entropy response messages
bool TestServerGenerateEntropyResponse();

// A test to validate config counting of active servers
bool TestConfigActiveServerCount();

// A test to validate config generation of a flat server list
bool TestConfigGetServerList();
