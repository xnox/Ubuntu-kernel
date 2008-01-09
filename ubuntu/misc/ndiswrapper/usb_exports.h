/* automatically generated from usb.c */
#ifdef CONFIG_X86_64

WIN_FUNC_DECL(USBD_CreateConfigurationRequest, 2)
WIN_FUNC_DECL(USBD_CreateConfigurationRequestEx, 2)
WIN_FUNC_DECL(USBD_GetUSBDIVersion, 1)
WIN_FUNC_DECL(USBD_ParseConfigurationDescriptor, 3)
WIN_FUNC_DECL(USBD_ParseConfigurationDescriptorEx, 7)
WIN_FUNC_DECL(USBD_ParseDescriptors, 4)
WIN_FUNC_DECL(wrap_cancel_irp, 2)
#endif
struct wrap_export usb_exports[] = {
   
   {"_USBD_CreateConfigurationRequestEx@8",(generic_func)USBD_CreateConfigurationRequestEx},
   {"_USBD_ParseConfigurationDescriptorEx@28",(generic_func)USBD_ParseConfigurationDescriptorEx},
   {"_USBD_ParseDescriptors@16",(generic_func)USBD_ParseDescriptors},
   WIN_SYMBOL(USBD_CreateConfigurationRequest,2),
   WIN_SYMBOL(USBD_CreateConfigurationRequestEx,2),
   WIN_SYMBOL(USBD_GetUSBDIVersion,1),
   WIN_SYMBOL(USBD_ParseConfigurationDescriptor,3),
   WIN_SYMBOL(USBD_ParseConfigurationDescriptorEx,7),
   WIN_SYMBOL(USBD_ParseDescriptors,4),
   {NULL, NULL}
};
