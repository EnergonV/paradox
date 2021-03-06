﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.

using System;
using System.Collections.Generic;
using System.Linq;

using SharpYaml;
using SharpYaml.Events;
using SharpYaml.Serialization;
using SharpYaml.Serialization.Descriptors;
using SharpYaml.Serialization.Logging;
using SharpYaml.Serialization.Serializers;
using SiliconStudio.Core.Yaml;
using SiliconStudio.Paradox.Engine;
using SiliconStudio.Paradox.Engine.Design;

namespace SiliconStudio.Paradox.Assets.Serializers
{
    /// <summary>
    /// Error resistant Script loading. It should work even if there is missing properties, members or types.
    /// If main script type is missing (usually due to broken assemblies), it will keep the Yaml representation so that it can be properly saved alter.
    /// </summary>
    [YamlSerializerFactory]
    internal class ScriptCollectionSerializer : CollectionSerializer
    {
        public override IYamlSerializable TryCreate(SerializerContext context, ITypeDescriptor typeDescriptor)
        {
            var type = typeDescriptor.Type;
            return type == typeof(ScriptCollection) ? this : null;
        }

        protected override void ReadAddCollectionItem(ref ObjectContext objectContext, Type elementType, CollectionDescriptor collectionDescriptor, object thisObject, int index)
        {
            var scriptCollection = (ScriptCollection)objectContext.Instance;

            object value = null;
            bool needAdd = true; // If we could get existing value, no need add to collection
            if (scriptCollection.Count > index)
            {
                value = scriptCollection[index];
                needAdd = false;
            }

            value = ReadCollectionItem(ref objectContext, value, elementType);
            if (needAdd)
                collectionDescriptor.CollectionAdd(thisObject, value);
        }

        /// <inheritdoc/>
        protected override object ReadCollectionItem(ref ObjectContext objectContext, object value, Type itemType)
        {
            // Save the Yaml stream, in case loading fails we can keep this representation
            var parsingEvents = new List<ParsingEvent>();
            var reader = objectContext.Reader;
            var startDepth = reader.CurrentDepth;
            do
            {
                parsingEvents.Add(reader.Expect<ParsingEvent>());
            } while (reader.CurrentDepth > startDepth);

            // Save states
            var previousReader = objectContext.SerializerContext.Reader;
            var previousAllowErrors = objectContext.SerializerContext.AllowErrors;

            objectContext.SerializerContext.Reader = new EventReader(new MemoryParser(parsingEvents));
            objectContext.SerializerContext.AllowErrors = true;

            try
            {
                return objectContext.ObjectSerializerBackend.ReadCollectionItem(ref objectContext, value, itemType);
            }
            catch (YamlException ex)
            {
                // There was a failure, let's keep this object so that it can be serialized back later
                var startEvent = parsingEvents.FirstOrDefault() as MappingStart;
                string typeName = startEvent != null && !string.IsNullOrEmpty(startEvent.Tag) ? startEvent.Tag.Substring(1) : null;

                var log = objectContext.SerializerContext.ContextSettings.Logger;
                if (log != null)
                    log.Log(LogLevel.Warning, ex, string.Format("Could not deserialize script {0}", typeName));

                return new UnloadableScript(parsingEvents, typeName);
            }
            finally
            {
                // Restore states
                objectContext.SerializerContext.Reader = previousReader;
                objectContext.SerializerContext.AllowErrors = previousAllowErrors;
            }
        }

        /// <inheritdoc/>
        protected override void WriteCollectionItem(ref ObjectContext objectContext, object item, Type itemType)
        {
            // Check if we have a Yaml representation (in case loading failed)
            var unloadableScript = item as UnloadableScript;
            if (unloadableScript != null)
            {
                var writer = objectContext.Writer;
                foreach (var parsingEvent in unloadableScript.ParsingEvents)
                {
                    writer.Emit(parsingEvent);
                }
                return;
            }

            base.WriteCollectionItem(ref objectContext, item, itemType);
        }
    }
}