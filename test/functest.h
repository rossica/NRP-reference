// A simple smoke test to verify that the platform's compiler isn't sticking
// padding into the structures. If it is, this indicates that this platform
// needs to be special cased, and the structs re-defined to work.
bool TestStructSizes();

// A test to validate the request creation and validation functions
bool TestCreateRequest();

// A test to validate the response creation and validation functions
bool TestCreateResponse();
