#include <ntifs.h>
#include <cstdint>

#define DEV_NAME L"\\Device\\WmapDriver"
#define SYM_LINK_NAME L"\\??\\Wmap"

#define IOCTL_BASE 0x800
#define IOCTL_CODE(i) CTL_CODE(FILE_DEVICE_UNKNOWN,IOCTL_BASE+i,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define IOCTL_COPY_MEMORY IOCTL_CODE(1)
#define IOCTL_FILL_MEMORY IOCTL_CODE(2)
#define IOCTL_GET_PHYS_ADDRESS IOCTL_CODE(3)
#define IOCTL_MAP_IO_SPACE IOCTL_CODE(4)
#define IOCTL_UNMAP_IO_SPACE IOCTL_CODE(5)

typedef struct WRIO
{
	uint64_t source;
	uint64_t destination;
	uint64_t length;
	uint64_t address_to_translate;
	uint64_t return_physical_address;
	uint64_t physical_address_to_map;
	uint64_t return_virtual_address;
	uint64_t virt_address;
	uint32_t value;
	uint32_t size;
	uint32_t number_of_bytes;
}WRIO, * PWRIO;

NTSTATUS CreateMyDevice(IN PDRIVER_OBJECT pDriverObject) {
	NTSTATUS status;
	UNICODE_STRING devName;				//设备名称
	UNICODE_STRING sysLinkName;			//系统符号链接名
	PDEVICE_OBJECT pDevObject;				//用于返回创建设备

	RtlInitUnicodeString(&devName, DEV_NAME);
	status = IoCreateDevice(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &pDevObject);
	if (!NT_SUCCESS(status)) {						//判断创建设备是否成功
		if (status == STATUS_INSUFFICIENT_RESOURCES)
			KdPrint(("资源不足\n"));
		if (status == STATUS_OBJECT_NAME_EXISTS)
			KdPrint(("指定对象名存在\n"));
		if (status == STATUS_OBJECT_NAME_COLLISION)
			KdPrint(("对象名有冲突\n"));
		return status;
	}
	KdPrint(("设备创建成功\n"));

	pDevObject->Flags |= DO_BUFFERED_IO;	//缓冲区方式读写
	RtlInitUnicodeString(&sysLinkName, SYM_LINK_NAME);
	IoDeleteSymbolicLink(&sysLinkName);		//防止已有相同符号链接重复
	status = IoCreateSymbolicLink(&sysLinkName, &devName);		//判断生成符号链接是否成功

	if (!NT_SUCCESS(status)) {
		KdPrint(("生成符号链接失败\n"));
		IoDeleteDevice(pDevObject);
		return status;
	}
	KdPrint(("生成符号链接成功\n"));
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObject) {
	PDEVICE_OBJECT pDevObject;
	UNICODE_STRING sysLinkName;

	pDevObject = pDriverObject->DeviceObject;
	IoDeleteDevice(pDevObject);	//取得设备并删除
	KdPrint(("成功删除设备\n"));

	RtlInitUnicodeString(&sysLinkName, SYM_LINK_NAME);
	IoDeleteSymbolicLink(&sysLinkName);	//取得符号链接并删除
	KdPrint(("成功删除符号链接\n"));

	KdPrint(("驱动成功卸载\n"));
}

NTSTATUS OtherCompleteRoutine(PDEVICE_OBJECT pDriverObj, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ControlDispatchRoutine(IN PDEVICE_OBJECT pDevobj, IN PIRP pIrp) {
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG_PTR InLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;   //获取输入缓冲区大小
	ULONG_PTR OutLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength; //获取输出缓冲区大小
	ULONG_PTR IoControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;  //得到IOCTL码
	PWRIO pIoBuffer = reinterpret_cast<PWRIO>(pIrp->AssociatedIrp.SystemBuffer);
	ULONG_PTR info = 0;
	NTSTATUS status = STATUS_SUCCESS;

	switch (IoControlCode)
	{
	case IOCTL_COPY_MEMORY:
	{
		KdPrint(("复制内存"));
		RtlCopyMemory((void*)pIoBuffer->destination, (void*)pIoBuffer->source, pIoBuffer->length);
	}
	break;
	case IOCTL_FILL_MEMORY:
	{
		KdPrint(("填充内存"));
		RtlFillMemory((void*)pIoBuffer->destination, pIoBuffer->length, pIoBuffer->value);
	}
	break;
	case IOCTL_GET_PHYS_ADDRESS:
	{
		KdPrint(("获取物理地址内存"));
		pIoBuffer->return_physical_address = (uint64_t)MmGetPhysicalAddress((void*)pIoBuffer->address_to_translate).QuadPart;
		info = sizeof(WRIO);
	}
	break;
	case IOCTL_MAP_IO_SPACE:
	{
		KdPrint(("映射内存"));
		PHYSICAL_ADDRESS physicalAddress{ 0 };
		physicalAddress.QuadPart = pIoBuffer->physical_address_to_map;

		pIoBuffer->return_virtual_address = (uint64_t)MmMapIoSpace(physicalAddress, pIoBuffer->size, MmNonCached);
		info = sizeof(WRIO);
	}
	break;
	case IOCTL_UNMAP_IO_SPACE:
	{
		KdPrint(("取消映射内存"));
		MmUnmapIoSpace((void*)pIoBuffer->virt_address, pIoBuffer->number_of_bytes);
	}
	break;
	default:
		KdPrint(("其它处理"));
		break;
	}

	pIrp->IoStatus.Information = info;			//设置操作的字节数为0，这里无实际意义
	pIrp->IoStatus.Status = STATUS_SUCCESS;		//返回成功
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);	//指示完成此IRP
	KdPrint(("离开派遣函数\n"));				//调试信息
	return STATUS_SUCCESS;
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING reg_path) {
	KdPrint(("驱动成功加载\n"));
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = OtherCompleteRoutine;	//创建派遣例程，也可分开用不同函数
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_READ] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = OtherCompleteRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ControlDispatchRoutine;
	CreateMyDevice(pDriverObject);
	pDriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}