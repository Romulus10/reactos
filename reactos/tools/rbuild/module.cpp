// module.cpp

#include "pch.h"
#include <assert.h>

#include "rbuild.h"

using std::string;
using std::vector;

string
FixSeparator ( const string& s )
{
	string s2(s);
	char* p = strchr ( &s2[0], CBAD_SEP );
	while ( p )
	{
		*p++ = CSEP;
		p = strchr ( p, CBAD_SEP );
	}
	return s2;
}

Module::Module ( const Project& project,
                 const XMLElement& moduleNode,
                 const string& modulePath )
	: project(project),
	  node(moduleNode)
{
	if ( node.name != "module" )
		throw Exception ( "internal tool error: Module created with non-<module> node" );

	path = FixSeparator ( modulePath );

	const XMLAttribute* att = moduleNode.GetAttribute ( "name", true );
	assert(att);
	name = att->value;

	att = moduleNode.GetAttribute ( "type", true );
	assert(att);
	type = GetModuleType ( *att );

	att = moduleNode.GetAttribute ( "extension", false );
	if (att != NULL)
		extension = att->value;
	else
		extension = GetDefaultModuleExtension ();
}

Module::~Module ()
{
	size_t i;
	for ( i = 0; i < files.size(); i++ )
		delete files[i];
	for ( i = 0; i < libraries.size(); i++ )
		delete libraries[i];
	for ( i = 0; i < includes.size(); i++ )
		delete includes[i];
	for ( i = 0; i < defines.size(); i++ )
		delete defines[i];
	for ( i = 0; i < invocations.size(); i++ )
		delete invocations[i];
	for ( i = 0; i < dependencies.size(); i++ )
		delete dependencies[i];
}

void
Module::ProcessXML()
{
	size_t i;
	for ( i = 0; i < node.subElements.size(); i++ )
		ProcessXMLSubElement ( *node.subElements[i], path );
	for ( i = 0; i < files.size (); i++ )
		files[i]->ProcessXML ();
	for ( i = 0; i < libraries.size(); i++ )
		libraries[i]->ProcessXML ();
	for ( i = 0; i < includes.size(); i++ )
		includes[i]->ProcessXML ();
	for ( i = 0; i < defines.size(); i++ )
		defines[i]->ProcessXML ();
	for ( i = 0; i < invocations.size(); i++ )
		invocations[i]->ProcessXML ();
	for ( i = 0; i < dependencies.size(); i++ )
		dependencies[i]->ProcessXML ();
}

void
Module::ProcessXMLSubElement ( const XMLElement& e,
                               const string& path )
{
	bool subs_invalid = false;
	string subpath ( path );
	if ( e.name == "file" && e.value.size () > 0 )
	{
		files.push_back ( new File ( FixSeparator ( path + CSEP + e.value ) ) );
		subs_invalid = true;
	}
	else if ( e.name == "library" && e.value.size () )
	{
		libraries.push_back ( new Library ( e, *this, e.value ) );
		subs_invalid = true;
	}
	else if ( e.name == "directory" )
	{
		const XMLAttribute* att = e.GetAttribute ( "name", true );
		assert(att);
		subpath = FixSeparator ( path + CSEP + att->value );
	}
	else if ( e.name == "include" )
	{
		includes.push_back ( new Include ( project, this, e ) );
		subs_invalid = true;
	}
	else if ( e.name == "define" )
	{
		defines.push_back ( new Define ( project, this, e ) );
		subs_invalid = true;
	}
	else if ( e.name == "invoke" )
	{
		invocations.push_back ( new Invoke ( e, *this ) );
		subs_invalid = false;
	}
	else if ( e.name == "dependency" )
	{
		dependencies.push_back ( new Dependency ( e, *this ) );
		subs_invalid = true;
	}
	if ( subs_invalid && e.subElements.size() > 0 )
		throw InvalidBuildFileException (
			e.location,
			"<%s> cannot have sub-elements",
			e.name.c_str() );
	for ( size_t i = 0; i < e.subElements.size (); i++ )
		ProcessXMLSubElement ( *e.subElements[i], subpath );
}

ModuleType
Module::GetModuleType ( const XMLAttribute& attribute )
{
	if ( attribute.value == "buildtool" )
		return BuildTool;
	if ( attribute.value == "staticlibrary" )
		return StaticLibrary;
	if ( attribute.value == "kernelmodedll" )
		return KernelModeDLL;
	throw InvalidAttributeValueException ( attribute.name,
	                                       attribute.value );
}

string
Module::GetDefaultModuleExtension () const
{
	switch (type)
	{
		case BuildTool:
			return EXEPOSTFIX;
		case StaticLibrary:
			return ".a";
		case KernelModeDLL:
			return ".dll";
	}
	throw InvalidOperationException (__FILE__,
	                                 __LINE__);
}

string
Module::GetBasePath() const
{
	return path;
}

string
Module::GetPath () const
{
	return path + CSEP + name + extension;
}

string
Module::GetTargets () const
{
	if ( invocations.size () > 0 )
	{
		string targets ( "" );
		for ( size_t i = 0; i < invocations.size (); i++ )
		{
			Invoke& invoke = *invocations[i];
			if ( targets.length () > 0 )
				targets += " ";
			targets += invoke.GetTargets ();
		}
		return targets;
	}
	else
		return GetPath ();
}

string
Module::GetInvocationTarget ( const int index ) const
{
	return ssprintf ( "%s_invoke_%d",
	                  name.c_str (),
	                  index );
}


File::File ( const string& _name )
	: name(_name)
{
}

void
File::ProcessXML()
{
}


Library::Library ( const XMLElement& _node,
                   const Module& _module,
                   const string& _name )
	: node(_node),
	  module(_module),
	  name(_name)
{
	if ( module.name == name )
		throw InvalidBuildFileException (
			node.location,
			"module '%s' cannot link against itself",
			name.c_str() );
}

void
Library::ProcessXML()
{
	if ( !module.project.LocateModule ( name ) )
		throw InvalidBuildFileException (
			node.location,
			"module '%s' trying to link against non-existant module '%s'",
			module.name.c_str(),
			name.c_str() );
}


Invoke::Invoke ( const XMLElement& _node,
                 const Module& _module )
	: node(_node),
	  module(_module)
{
}

void
Invoke::ProcessXML()
{
	for ( size_t i = 0; i < node.subElements.size (); i++ )
		ProcessXMLSubElement ( *node.subElements[i] );
}

void
Invoke::ProcessXMLSubElement ( const XMLElement& e )
{
	bool subs_invalid = false;
	if ( e.name == "output" )
	{
		for ( size_t i = 0; i < e.subElements.size (); i++ )
			ProcessXMLSubElementOutput ( *e.subElements[i] );
	}
	if ( subs_invalid && e.subElements.size() > 0 )
		throw InvalidBuildFileException ( e.location,
		                                  "<%s> cannot have sub-elements",
		                                  e.name.c_str() );
}

void
Invoke::ProcessXMLSubElementOutput ( const XMLElement& e )
{
	bool subs_invalid = false;
	if ( e.name == "outputfile" && e.value.size () > 0 )
	{
		output.push_back ( new File ( FixSeparator ( module.path + CSEP + e.value ) ) );
		subs_invalid = true;
	}
	if ( subs_invalid && e.subElements.size() > 0 )
		throw InvalidBuildFileException ( e.location,
		                                  "<%s> cannot have sub-elements",
		                                  e.name.c_str() );
}

string
Invoke::GetTargets () const
{
	string targets ( "" );
	for ( size_t i = 0; i < output.size (); i++ )
	{
		File& file = *output[i];
		if ( targets.length () > 0 )
			targets += " ";
		targets += file.name;
	}
	return targets;
}


Dependency::Dependency ( const XMLElement& _node,
                         const Module& _module )
	: node (_node),
	  module (_module),
	  dependencyModule (NULL)
{
}

void
Dependency::ProcessXML()
{
	dependencyModule = module.project.LocateModule ( node.value );
	if ( dependencyModule == NULL )
		throw InvalidBuildFileException ( node.location,
		                                  "module '%s' depend on non-existant module '%s'",
		                                  module.name.c_str(),
		                                  node.value.c_str() );
}
