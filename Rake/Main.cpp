#include <Luna.hpp>

#include "Rake.hpp"

int main(int argc, const char** argv) {
	auto app    = std::make_unique<Rake>();
	auto engine = std::make_unique<Luna::Engine>();
	engine->SetApp(app.get());

	return engine->Run();
}
