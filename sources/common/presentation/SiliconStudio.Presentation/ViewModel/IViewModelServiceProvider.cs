﻿// Copyright (c) 2014 Silicon Studio Corp. (http://siliconstudio.co.jp)
// This file is distributed under GPL v3. See LICENSE.md for details.
using System;

namespace SiliconStudio.Presentation.ViewModel
{
    /// <summary>
    /// A service provider class for view model objects.
    /// </summary>
    public interface IViewModelServiceProvider
    {
        /// <summary>
        /// Register a new service in this <see cref="ViewModelServiceProvider"/>.
        /// </summary>
        /// <param name="service">The service to register.</param>
        /// <exception cref="ArgumentNullException"><c>service</c> is null.</exception>
        /// <exception cref="InvalidOperationException">A service of the same type has already been registered.</exception>
        void RegisterService(object service);

        /// <summary>
        /// Unregister a service from this <see cref="ViewModelServiceProvider"/>.
        /// </summary>
        /// <param name="service">The service to unregister.</param>
        /// <exception cref="ArgumentNullException"><c>service</c> is null.</exception>
        void UnregisterService(object service);

            /// <summary>
        /// Gets a service of the given type, if available.
        /// </summary>
        /// <param name="serviceType">The type of service to retrieve.</param>
        /// <returns>An instance of the service that match the given type if available, <c>null</c> otherwise.</returns>
        /// <exception cref="InvalidOperationException">Multiple services match the given type.</exception>
        object TryGet(Type serviceType);

        /// <summary>
        /// Gets a service of the given type, if available.
        /// </summary>
        /// <typeparam name="T">The type of service to retrieve.</typeparam>
        /// <returns>An instance of the service that match the given type if available, <c>null</c> otherwise.</returns>
        /// <exception cref="InvalidOperationException">Multiple services match the given type.</exception>
        T TryGet<T>() where T : class;

        /// <summary>
        /// Gets a service of the given type.
        /// </summary>
        /// <param name="serviceType">The type of service to retrieve.</param>
        /// <returns>An instance of the service that match the given type.</returns>
        /// <exception cref="InvalidOperationException">No service matches the given type.</exception>
        /// <exception cref="InvalidOperationException">Multiple services match the given type.</exception>
        object Get(Type serviceType);

        /// <summary>
        /// Gets a service of the given type.
        /// </summary>
        /// <typeparam name="T">The type of service to retrieve.</typeparam>
        /// <returns>An instance of the service that match the given type.</returns>
        /// <exception cref="InvalidOperationException">No service matches the given type.</exception>
        /// <exception cref="InvalidOperationException">Multiple services match the given type.</exception>
        T Get<T>() where T : class;
    }
}